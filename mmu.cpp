#include <iostream>
#include <fstream>
#include <deque>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <set>
#include <math.h>
#include <unistd.h> 

#define  pidvpn_to_pte(pid,vpn)    (procList[pid].page_table[vpn])

using namespace std;

class Pstat;
class Process;
class Pager;
class FIFO;
class Clock;
class NRU;
class Random;
class Aging;
class WorkingSet;
int myrandom();

typedef struct {
    unsigned int PRESENT:1;
    unsigned int REFERENCED:1;
    unsigned int MODIFIED:1;
    unsigned int WRITE_PROTECT:1;
    unsigned int PAGEDOUT:1;
    unsigned int FRAME_ADDR:7;
    unsigned int ACCESSED:1; // Has this pte been accessed?
    unsigned int VALID:1; // is this pte valid ?
    unsigned int MAPPED:1;
} pte_t;

typedef struct {
    unsigned int FRAME_ADDR:7;
    unsigned int VPAGE_ADDR:6;
    unsigned int PID: 4;
    unsigned int ACCESSED: 1;
    unsigned AGE;
    unsigned long lastTime;
} frame_t;

typedef struct {
    int start_virtual_page;
    int end_virtual_page;
    int write_protected;
    int filemapped;
} vma;

typedef struct {
    char inst;
    int vpage_id;
} instruction;


// define const global variables
int MAX_FRAMES = 32;
const static int MAX_VPAGES = 64;
const static string MAP = "MAP";
const static string UNMAP = "UNMAP";
const static string FIN = "FIN";
const static string IN = "IN";
const static string FOUT = "FOUT";
const static string OUT = "OUT";
const static string SEGV = "SEGV";
const static string SEGPROT = "SEGPROT";
const static string ZERO = "ZERO";
const static string FIFO_alg = "FIFO";
const static string Clock_alg = "Clock";
const static string NRU_alg = "NRU";
const static string Random_alg = "Random";
const static string Aging_alg = "Aging";
const static string WorkingSet_alg = "WorkingSet";

const static int TAU = 49;
const static int RW_cost = 1;
const static int switch_cost = 130;
const static int exit_cost = 1250;
const static int M_cost = 300;
const static int U_cost = 400;
const static int I_cost = 3100;
const static int O_cost = 2700;
const static int FI_cost = 2800;
const static int FO_cost = 2400;
const static int Z_cost = 140;
const static int SV_cost = 340;
const static int SP_cost = 420;



unsigned long inst_count = 0;
unsigned long ctx_switches = 0;
unsigned long process_exits = 0;
unsigned long long cost = 0;


string INPUT_FILE = "./inputs/in10";
string RFILE;
deque<int> randvals;
vector<Process> procList;
vector<Pstat> pstatList;
deque<instruction> instList;
deque<frame_t *> free_pool;
frame_t *frame_table;
Pager* THE_PAGER;
Process* current_process;
pte_t* current_page;
int RM_class[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
int last_inst_cnt = 0;
char alg_type;
bool whetherO=false, whetherP=false, whetherF=false, whetherS=false;

class Pstat {
    public:
    unsigned long U;
    unsigned long M;
    unsigned long I;
    unsigned long O;
    unsigned long FI;
    unsigned long FO;
    unsigned long Z;
    unsigned long SV;
    unsigned long SP;
    Pstat() {
        U = 0;
        M = 0;
        I = 0;
        O = 0;
        FI = 0;
        FO = 0;
        Z = 0;
        SV = 0;
        SP = 0;
    }
};

class Process {
    public:
    int _pid;
    int _vma_num;
    deque<vma> vma_l;
    pte_t page_table[MAX_VPAGES];
    Process(int pid, int vma_num) {
        _pid = pid;
        _vma_num = vma_num;
    }
    void addVma(int start_virtual_page, int end_virtual_page, int write_protected, int filemapped) {
        vma nextVma = {
            .start_virtual_page = start_virtual_page,
            .end_virtual_page = end_virtual_page,
            .write_protected = write_protected,
            .filemapped = filemapped
        };
        vma_l.push_back(nextVma);
        for(int i=0; i<MAX_VPAGES; i++) {
            pte_t *pte = &page_table[i];
            pte->REFERENCED = 0;
            pte->PRESENT = 0;
            pte->MODIFIED = 0;
            pte->WRITE_PROTECT = 0;
            pte->PAGEDOUT = 0;
            pte->FRAME_ADDR = 0;
            pte->ACCESSED = 0;
            pte->VALID = 0;
            pte->MAPPED = 0;
        }
    }
    bool validatePage(int vpage) {
        for(int i=0; i<vma_l.size(); i++) {
            if(vma_l[i].start_virtual_page<=vpage&&vma_l[i].end_virtual_page>=vpage) {
                page_table[vpage].ACCESSED = 1;
                page_table[vpage].MAPPED = vma_l[i].filemapped;
                page_table[vpage].VALID = 1;
                page_table[vpage].WRITE_PROTECT = vma_l[i].write_protected;
                return true;
            }
        }
        page_table[vpage].ACCESSED = 1;
        page_table[vpage].VALID = 0;
        return false;
    }

};

class Pager { 
    public:
        int hand;
        virtual frame_t* select_victim_frame() = 0; // virtual base class
        virtual void resetAge(int target) = 0;
};

class FIFO : public Pager{
    public:
        int hand;
        FIFO() {
            hand = 0;
        }
        frame_t *select_victim_frame() {
            frame_t *rst = &frame_table[hand++];
            hand = hand%MAX_FRAMES;
            return rst;
        }
        void resetAge(int target) {

        }
};

class Clock : public Pager {
    public:
        int hand;
        Clock() {
            hand = 0;
        }
        frame_t *select_victim_frame() {
            while(pidvpn_to_pte(frame_table[hand].PID, frame_table[hand].VPAGE_ADDR).REFERENCED==1) {
                (&pidvpn_to_pte(frame_table[hand].PID, frame_table[hand].VPAGE_ADDR))->REFERENCED = 0;
                hand = (hand+1)%MAX_FRAMES;
            }
            frame_t *rst = &frame_table[hand++];
            hand = hand%MAX_FRAMES;
            return rst;
        }
        void resetAge(int target) {

        }

};

class NRU : public Pager {
    public:
        int hand;
        NRU() {
            hand = 0;
        }
        frame_t *select_victim_frame() {
            for(int i=0; i<4; i++) {
                for(int j=0; j<MAX_FRAMES; j++) {
                    int index = (hand+j)%MAX_FRAMES;
                    pte_t pte = pidvpn_to_pte(frame_table[index].PID, frame_table[index].VPAGE_ADDR);
                    if(pte.REFERENCED==RM_class[i][0]&&pte.MODIFIED==RM_class[i][1]) {
                        hand = (index+1)%MAX_FRAMES;
                        if(inst_count-last_inst_cnt>=50) {
                            for(int z=0; z<MAX_FRAMES; z++) {
                                int cur_index = (hand+z)%MAX_FRAMES;
                               (&pidvpn_to_pte(frame_table[cur_index].PID, frame_table[cur_index].VPAGE_ADDR))->REFERENCED = 0;
                            }
                            last_inst_cnt = inst_count;
                        }
                        return &frame_table[index];
                    }
                }
            }
            return &frame_table[hand];
        }
        void resetAge(int target) {

        }
};

class Random : public Pager {
    public:
        frame_t *select_victim_frame() {
            return &frame_table[myrandom()];
        }
        void resetAge(int target) {

        }
};

class Aging : public Pager {
    public:
        int hand;
        Aging() {
            hand = 0;
        }
        frame_t *select_victim_frame() {
            int index = hand;
            for(int i=0; i<MAX_FRAMES; i++) {
                int curIndex = (hand+i)%MAX_FRAMES;
                frame_t *curFrame = &frame_table[curIndex];
                curFrame->AGE >>= 1;
                if(pidvpn_to_pte(curFrame->PID, curFrame->VPAGE_ADDR).REFERENCED) {
                    curFrame->AGE = (curFrame->AGE | 0x80000000);
                }
                (&pidvpn_to_pte(curFrame->PID, curFrame->VPAGE_ADDR))->REFERENCED = 0;
                if(curFrame->AGE<frame_table[index].AGE) {
                    index = curIndex;
                }
            }
            frame_t *rst = &frame_table[index];
            hand = (index+1)%MAX_FRAMES;
            return rst;
        }
        void resetAge(int target) {
            (&frame_table[target])->AGE = 0;
        }

};

class WorkingSet : public Pager {
    public:
        int hand;
        WorkingSet() {
            hand = 0;
        }
        frame_t *select_victim_frame() {
            int index = hand;
            for(int i=0; i<MAX_FRAMES; i++) {
                int curIndex = (hand+i)%MAX_FRAMES;
                frame_t *curFrame = &frame_table[curIndex];
                pte_t *curPte = &pidvpn_to_pte(curFrame->PID, curFrame->VPAGE_ADDR);
                if(curPte->REFERENCED) {
                    curFrame->lastTime = inst_count;
                    curPte->REFERENCED = 0;
                } else {
                    if(inst_count-curFrame->lastTime>TAU) {
                        hand = (curIndex+1)%MAX_FRAMES;
                        return curFrame;
                    } else {
                        if(curFrame->lastTime<frame_table[index].lastTime) {
                            index = curIndex;
                        }
                    }
                }
            }
            hand = (index+1)%MAX_FRAMES;
            return &frame_table[index];
        }
        void resetAge(int target) {

        }
};

frame_t *allocate_frame_from_free_list() {
    if(free_pool.empty()) {
        return NULL;
    } else {
        frame_t *rst = free_pool.front();
        free_pool.pop_front();
        return rst;
    }
}

frame_t *get_frame() { 
    frame_t *frame = allocate_frame_from_free_list(); 
    if (frame == NULL) frame = THE_PAGER->select_victim_frame(); 
    return frame; 
}


void checkInputFile() {
    for(int i=0; i<procList.size(); i++) {
        printf("%d %d\n", procList[i]._pid, procList[i]._vma_num);
        for(int j=0; j<procList[i]._vma_num; j++) {
            printf("%d %d %d %d\n", procList[i].vma_l[j].start_virtual_page, procList[i].vma_l[j].end_virtual_page, procList[i].vma_l[j].write_protected, procList[i].vma_l[j].filemapped);
        }
    }
    for(int i=0; i<instList.size(); i++) {
        printf("%c %d\n", instList[i].inst, instList[i].vpage_id);
    }
}

void checkRFile() {
    for(int i=0; i<randvals.size(); i++) {
        printf("%d\n", randvals[i]);
    }
}

bool get_next_instruction(char *operation, int *vpage) {
    if(instList.empty()) {
        return false;
    } else {
        instruction nextInst = instList.front();
        *operation = nextInst.inst;
        *vpage = nextInst.vpage_id;
        instList.pop_front();
        return true;
    }
}

int myrandom() { 
    int nextRand = randvals.front();
    int rst = nextRand % MAX_FRAMES; 
    randvals.pop_front();
    randvals.push_back(nextRand);
    return rst;
}


void Simulation() {
    char operation;
    int vpage = 0;
    while (get_next_instruction(&operation, &vpage)) {
        // TODO: handle special case of “c” and “e” instruction 
        if(whetherO) {
            printf("%lu: ==> %c %d\n", inst_count++, operation, vpage);
        }
        if(operation=='c') {
            current_process = &procList[vpage];
            ctx_switches += 1;
            cost += switch_cost;
            continue;
        }
        if(operation=='e') {
            process_exits += 1;
            cost += exit_cost;
            if(whetherO) {
                printf("EXIT current process %d\n", current_process->_pid);
            }
            set<int> unmap_frame_set;
            for(int i=0; i<MAX_VPAGES; i++) {
                pte_t *curpte = &pidvpn_to_pte(current_process->_pid, i);
                if(current_process->validatePage(i)) {
                    if(current_process->page_table[i].PRESENT) {
                        frame_t *curframe = &frame_table[curpte->FRAME_ADDR];
                        unmap_frame_set.insert(curframe->FRAME_ADDR);
                        pstatList[current_process->_pid].U += 1;
                        cost += U_cost;
                        if(whetherO) {
                            printf(" %s %d:%d\n", UNMAP.c_str(), curframe->PID, curframe->VPAGE_ADDR);
                        }
                        free_pool.push_back(curframe);
                        if(curpte->MODIFIED&&curpte->MAPPED) {
                            pstatList[current_process->_pid].FO += 1;
                            cost += FO_cost;
                            if(whetherO) {
                                printf(" %s\n", FOUT.c_str());
                            }
                        }
                        curframe->PID = 0;
                        curframe->VPAGE_ADDR = 0;
                        curframe->ACCESSED = 0;
                        curframe->AGE = 0;
                        curframe->lastTime = 0;
                    }
                }
                curpte->REFERENCED = 0;
                curpte->PRESENT = 0;
                curpte->MODIFIED = 0;
                curpte->WRITE_PROTECT = 0;
                curpte->PAGEDOUT = 0;
                curpte->FRAME_ADDR = 0;
                curpte->ACCESSED = 0;
                curpte->VALID = 0;
                curpte->MAPPED = 0;
            }
            current_process = NULL;
            continue;
        }
        // now the real instructions for read and write
        pte_t* pte = &(current_process->page_table[vpage]);
        if(operation=='r'||operation=='w') {
            cost += 1;
            if(pte->PRESENT!=1) {
                // this pte can be invalid
                // this in reality generates the page fault exception and now you execute 
                // verify this is actually a valid page in a vma if not raise error and next inst
                if((pte->ACCESSED==0&&!current_process->validatePage(vpage))||(pte->ACCESSED==1&&pte->VALID==0)) { 
                        // raise SEGV
                        pstatList[current_process->_pid].SV += 1;
                        cost += SV_cost;
                        if(whetherO) {
                            printf(" %s\n", SEGV.c_str());
                        }
                        // next inst
                        continue;
                } 
                // assign new frame to the page
                frame_t *newframe = get_frame();

                pte_t *last_pte = &pidvpn_to_pte(newframe->PID, newframe->VPAGE_ADDR);
                if(newframe->ACCESSED==0) {
                    newframe->ACCESSED = 1;
                } else {
                    pstatList[newframe->PID].U += 1;
                    cost += U_cost;
                    if(whetherO) {
                        printf(" %s %d:%d\n", UNMAP.c_str(), newframe->PID, newframe->VPAGE_ADDR);
                    }
                    last_pte->PRESENT = 0;
                    if(last_pte->MODIFIED) {
                        // (note once the PAGEDOUT flag is set it will never be reset as it indicates there is content on the swap device
                        last_pte->MODIFIED = 0;
                        if(last_pte->MAPPED) {
                            cost += FO_cost;
                            pstatList[newframe->PID].FO += 1;
                            if(whetherO) {
                                printf(" %s\n", FOUT.c_str());
                            }
                        } else {
                            pstatList[newframe->PID].O += 1;
                            last_pte->PAGEDOUT = 1;
                            cost += O_cost;
                            if(whetherO) {
                                printf(" %s\n", OUT.c_str());
                            }
                        }
                    }
                }
                if(pte->PAGEDOUT==1) {
                    if(pte->MAPPED) {
                        cost += FI_cost;
                        pstatList[current_process->_pid].FI += 1;
                        if(whetherO) {
                            printf(" %s\n", FIN.c_str());
                        }
                    } else {
                        cost += I_cost;
                        pstatList[current_process->_pid].I += 1;
                        if(whetherO) {
                            printf(" %s\n", IN.c_str());
                        }
                    }
                } else {
                    // not pagedout before 
                    if(pte->MAPPED==0) {
                        cost += Z_cost;
                        pstatList[current_process->_pid].Z += 1;
                        if(whetherO) {
                            printf(" %s\n", ZERO.c_str());
                        }
                    } else {
                        cost += FI_cost;
                        pstatList[current_process->_pid].FI += 1;
                        if(whetherO) {
                            printf(" %s\n", FIN.c_str());
                        }
                    }
                }
                // MAP part
                // when you map a frame, you must set its time of last use to the current time
                newframe->lastTime = inst_count;
                pstatList[current_process->_pid].M += 1;
                cost += M_cost;
                if(whetherO) {
                    printf(" %s %d\n", MAP.c_str(), newframe->FRAME_ADDR);
                }
                THE_PAGER->resetAge(newframe->FRAME_ADDR);
                pte->FRAME_ADDR = newframe->FRAME_ADDR;
                newframe->PID = current_process->_pid;
                newframe->VPAGE_ADDR = vpage;
                pte->PRESENT = 1;
                // reeset ref for NRU algorithm
                // THE_PAGER->resetAge(newframe->FRAME_ADDR);
            }
            pte->REFERENCED = 1;  
            // the vpage is backed by a frame and the instruction can proceed in hardware
            if(operation=='w') {
                if(pte->WRITE_PROTECT) {
                    pte->REFERENCED = 1;
                    pstatList[current_process->_pid].SP += 1;
                    cost += SP_cost;
                    if(whetherO) {
                        printf(" %s\n", SEGPROT.c_str());
                    }
                } else {
                    pte->REFERENCED = 1;
                    pte->MODIFIED = 1;
                }
            }
            if(operation=='r') {
                pte->REFERENCED = 1;
            }
        }
    }
}

void Summary() {
    if(whetherP) {
        for(int i=0; i<procList.size(); i++) {
            printf("PT[%d]:", i);
            for(int j=0; j<MAX_VPAGES; j++) {
                pte_t pte = procList[i].page_table[j];
                if(pte.VALID==0||pte.PRESENT==0) {
                    if(pte.PAGEDOUT==1) {
                        printf(" #");
                    } else {
                        printf(" *");
                    }
                } else {
                    printf(" %d:%c%c%c", j, pte.REFERENCED?'R':'-', pte.MODIFIED?'M':'-', pte.PAGEDOUT?'S':'-');
                }
            }
            printf("\n");
        }
    }
    if(whetherF) {
        printf("FT:");
        for(int i=0; i<MAX_FRAMES; i++) {
            if(frame_table[i].ACCESSED==1) {
                printf(" %d:%d", frame_table[i].PID, frame_table[i].VPAGE_ADDR);
            } else {
                printf(" *");
            }
        }
        printf("\n");
    }
    if(whetherS) {
        for(int i=0; i<procList.size(); i++) {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n", i, pstatList[i].U, pstatList[i].M,pstatList[i].I,pstatList[i].O,pstatList[i].FI,pstatList[i].FO,pstatList[i].Z,pstatList[i].SV,pstatList[i].SP);
        }
        printf("TOTALCOST %lu %lu %lu %llu %lu\n", inst_count, ctx_switches, process_exits, cost, sizeof(pte_t));
    }
}

int main(int argc, char *argv[]) {
    // read input 
    string line;
    int c;
    int index;
    opterr = 0;
    while ((c = getopt (argc, argv, "fao:")) != -1) {
        switch (c)
        {
        case 'f':
            sscanf(optarg+1, "%d", &MAX_FRAMES);
            break;
        case 'a':
            sscanf(optarg+1, "%c", &alg_type);
        case 'o':
            string optionStr;
            sscanf(optarg+1, "%s", &optionStr);
            for(int i=0; i<optionStr.size(); i++) {
                char curC = optionStr.at(i);
                if(curC=='O') {
                    whetherO = true;
                } else if(curC=='P') {
                    whetherP = true;
                } else if(curC=='F') {
                    whetherF = true;
                } else if(curC=='S') {
                    whetherS = true;
                }
            }
            break;
        }
    }
    if(optind<argc) {
        INPUT_FILE = argv[optind];
    } else {
        cout << "Missing Input File!" << endl;
        return 0;
    }
    if(optind+1<argc) {
        RFILE = argv[optind+1];
    } else {
        cout << "Missing Rfile!" << endl;
        return 0;
    }

    if(alg_type=='f') {
        THE_PAGER = new FIFO();
    } else if(alg_type=='r') {
        THE_PAGER = new Random();
    } else if(alg_type=='c') {
        THE_PAGER = new Clock();
    } else if(alg_type=='e') {
        THE_PAGER = new NRU();
    } else if(alg_type=='a') {
        THE_PAGER = new Aging();
    } else if(alg_type=='w') {
        THE_PAGER = new WorkingSet();
    } else {
        printf("No such algorithm!\n");
    }

    frame_table = new frame_t[MAX_FRAMES];
    
    // read rand file 
    fstream randFile;
    randFile.open(RFILE, fstream::in);
    std::getline(randFile, line, '\n');
    int randCnt = stoi(line);
    while(!randFile.eof()) {
        std::getline(randFile, line, '\n');
        if(line.length()>0) {
            int randNum = stoi(line);
            randvals.push_back(randNum);
        }
    }
    randFile.close();
    // read input
    int pid = 0;
    fstream inputFile;
    inputFile.open(INPUT_FILE, fstream::in);
    line = "#";
    // ignore line start with #
    while(line.at(0)=='#') {
        std::getline(inputFile, line, '\n');
    }
    int processNum = stoi(line);
    line = "#";
    for(int i=0; i<processNum; i++) {
        while(line.at(0)=='#') {
            std::getline(inputFile, line, '\n');
        }
        int vmaNum = stoi(line);
        Process proc(pid++, vmaNum);
        Pstat pstat;
        pstatList.push_back(pstat);
        for(int i=0; i<vmaNum; i++) {
            std::getline(inputFile, line, '\n');
            // split string by delimiter white space
            int start_virtual_page, end_virtual_page, write_protected, filemapped;
            sscanf(line.c_str(), "%d %d %d %d", &start_virtual_page, &end_virtual_page, &write_protected, &filemapped);
            proc.addVma(start_virtual_page, end_virtual_page, write_protected, filemapped);
        }
        procList.push_back(proc);
        line = "#";
    }
    line = "#";
    while(line.at(0)=='#') {
        std::getline(inputFile, line, '\n');
    }
    while(!inputFile.eof()) {
        if(line.at(0)=='#') {
            break;
        }
        char inst;
        int vpage_id;
        sscanf(line.c_str(), "%c %d", &inst, &vpage_id);
        instruction nextInst = {
            .inst = inst,
            .vpage_id = vpage_id
        };
        instList.push_back(nextInst);
        
        std::getline(inputFile, line, '\n');
    }
    inputFile.close();
    // checkInputFile();
    // checkRFile();
    
    // initialize frame_table
    // initialize free pool
    for(int i=0; i<MAX_FRAMES; i++) {
        frame_table[i].FRAME_ADDR = i;
        frame_table[i].PID = 0;
        frame_table[i].VPAGE_ADDR = 0;
        frame_table[i].ACCESSED = 0;
        frame_table[i].AGE = 0;
        frame_table[i].lastTime = 0;
        free_pool.push_back(&frame_table[i]);
    }
    Simulation();
    Summary();
    
}
