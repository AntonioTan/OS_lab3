#include <iostream>
#include <fstream>
#include <deque>
#include <string>
#include <sstream>
#include <vector>
#include <stdlib.h>
#include <set>
#define  pidvpn_to_pte(pid,vpn)    (procList[pid].page_table[vpn])

using namespace std;

class Process;
class Pager;
class FIFO;

typedef struct {
    unsigned PRESENT:1;
    unsigned REFERENCED:1;
    unsigned MODIFIED:1;
    unsigned WRITE_PROTECT:1;
    unsigned PAGEDOUT:1;
    unsigned FRAME_ADDR:7;
    unsigned ACCESSED:1; // Has this pte been accessed?
    unsigned VALID:1; // is this pte valid ?
    unsigned MAPPED:1;
} pte_t;

typedef struct {
    unsigned FRAME_ADDR:7;
    unsigned VPAGE_ADDR:6;
    unsigned PID: 4;
    unsigned ACCESSED: 1;
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
const static int MAX_FRAMES = 16;
const static int MAX_VPAGES = 128;
const static string MAP = "MAP";
const static string UNMAP = "UNMAP";
const static string FIN = "FIN";
const static string IN = "IN";
const static string FOUT = "FOUT";
const static string OUT = "OUT";
const static string SEGV = "SEGV";
const static string SEGPROT = "SEGPROT";
const static string ZERO = "ZERO";


string INPUT_FILE = "./inputs/in11";
deque<int> randvals;
vector<Process> procList;
deque<instruction> instList;
deque<frame_t *> free_pool;
frame_t frame_table[MAX_FRAMES];
Pager* THE_PAGER;
Process* current_process;
pte_t* current_page;

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

void Simulation() {
    char operation;
    int vpage = 0;
    int inst_cnt = 0;
    while (get_next_instruction(&operation, &vpage)) {
        // TODO: handle special case of “c” and “e” instruction 
        printf("%d: ==> %c %d\n", inst_cnt++, operation, vpage);
        if(operation=='c') {
            current_process = &procList[vpage];
            continue;
        }
        if(operation=='e') {
            printf("EXIT current process %d\n", current_process->_pid);
            
            set<int> unmap_frame_set;
            for(int i=0; i<MAX_VPAGES; i++) {
                if(current_process->validatePage(i)) {
                    if(current_process->page_table[i].PRESENT) {
                        pte_t *curpte = &pidvpn_to_pte(current_process->_pid, i);
                        frame_t *curframe = &frame_table[curpte->FRAME_ADDR];
                        curframe->ACCESSED = 0;
                        unmap_frame_set.insert(curframe->FRAME_ADDR);
                        printf(" %s %d:%d\n", UNMAP.c_str(), curframe->PID, curframe->VPAGE_ADDR);
                        free_pool.push_back(curframe);
                        if(current_process->page_table[i].MODIFIED==1&&current_process->page_table[i].MAPPED) {
                            printf(" %s\n", FOUT.c_str());
                        }
                    }
                }
            }
            current_process = NULL;
            continue;
        }
        // now the real instructions for read and write
        pte_t* pte = &(current_process->page_table[vpage]);
        if(pte->PRESENT!=1) {
            // this pte can be invalid
            // this in reality generates the page fault exception and now you execute 
            // verify this is actually a valid page in a vma if not raise error and next inst
            if((pte->ACCESSED==0&&!current_process->validatePage(vpage))||(pte->ACCESSED==1&&pte->VALID==0)) { 
                    // raise SEGV
                    printf(" %s\n", "SEGV");
                    // next inst
                    continue;
            } 
            // assign new frame to the page
            frame_t *newframe = get_frame();

            pte_t *last_pte = &pidvpn_to_pte(newframe->PID, newframe->VPAGE_ADDR);
            if(newframe->ACCESSED==0) {
                newframe->ACCESSED = 1;
            } else {
                printf(" %s %d:%d\n", UNMAP.c_str(), newframe->PID, newframe->VPAGE_ADDR);
                last_pte->PRESENT = 0;
                if(last_pte->MODIFIED) {
                    // (note once the PAGEDOUT flag is set it will never be reset as it indicates there is content on the swap device
                    last_pte->PAGEDOUT = 1;
                    last_pte->MODIFIED = 0;
                    if(last_pte->MAPPED) {
                        printf(" %s\n", FOUT.c_str());
                    } else {
                        printf(" %s\n", OUT.c_str());
                    }
                }
            }
            if(pte->PAGEDOUT==1) {
                if(pte->MAPPED) {
                    printf(" %s\n", FIN.c_str());
                } else {
                    printf(" %s\n", IN.c_str());
                }
            } else {
                // not pagedout before 
                if(pte->MAPPED==0) {
                    printf(" %s\n", ZERO.c_str());
                } else {
                    printf(" %s\n", FIN.c_str());
                }
            }
            // MAP part
            printf(" %s %d\n", MAP.c_str(), newframe->FRAME_ADDR);
            pte->FRAME_ADDR = newframe->FRAME_ADDR;
            newframe->PID = current_process->_pid;
            newframe->VPAGE_ADDR = vpage;
            pte->PRESENT = 1;
        }
        // the vpage is backed by a frame and the instruction can proceed in hardware
        if(operation=='w') {
            if(pte->WRITE_PROTECT) {
                pte->REFERENCED = 1;
                printf(" %s\n", SEGPROT.c_str());
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

public void Summary() {
    
}

int main(int argc, char *argv[]) {
    // read input 
    string line;
    // read rand file 
    fstream randFile;
    randFile.open("./inputs/rfile", fstream::in);
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
        free_pool.push_back(&frame_table[i]);
    }
    THE_PAGER = new FIFO();
    Simulation();
    Summary();
    
}
