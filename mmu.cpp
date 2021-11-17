#include <iostream>
#include <fstream>
#include <deque>
#include <string>
#include <sstream>
#include <vector>
#define  pidvpn_to_pte(pid,vpn)    (procList[pid].page_table[vpn])

using namespace std;

class Process;
class Pager;

typedef struct {
    unsigned PRESENT:1;
    unsigned REFERENCED:1;
    unsigned MODIFIED:1;
    unsigned WRITE_PROTECT:1;
    unsigned PAGEDOUT:1;
    unsigned FRAME_ADDR:7;
} pte_t;

typedef struct {
    unsigned FRAME_ADDR:7;
    unsigned VPAGE_ADDR:6;
} frame_t;

typedef struct {
    int start_virtual_page;
    int end_virtual_page;
    int write_protected;
    int filemapped;
} vma;

typedef struct {
    string inst;
    int vpage_id;
} instruction;

// define const global variables
const static int MAX_FRAMES = 128;
const static int MAX_VPAGES = 64;
string INPUT_FILE = "./inputs/in11";
deque<int> randvals;
vector<Process> procList;
deque<instruction> instList;
deque<frame_t> free_pool;
frame_t frame_table[MAX_FRAMES];
Pager* THE_PAGER;

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
    }
};

class Pager { 
    virtual frame_t* select_victim_frame() = 0; // virtual base class
};

frame_t *allocate_frame_from_free_list() {
    if(free_pool.empty()) {
        return NULL;
    } else {
        frame_t *rst = &free_pool.front();
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
        printf("%s %d\n", instList[i].inst.c_str(), instList[i].vpage_id);
    }
}

void checkRFile() {
    for(int i=0; i<randvals.size(); i++) {
        printf("%d\n", randvals[i]);
    }
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
            istringstream iss(line);
            vector<string> tokens;
            copy(istream_iterator<string>(iss),
                istream_iterator<string>(),
                back_inserter(tokens));
            if(tokens.size()==4) {
                int start_virtual_page = stoi(tokens[0]);
                int end_virtual_page = stoi(tokens[1]);
                int write_protected = stoi(tokens[2]);
                int filemapped = stoi(tokens[3]);
                proc.addVma(start_virtual_page, end_virtual_page, write_protected, filemapped);
            }
        }
        procList.push_back(proc);
        line = "#";
    }
    line = "#";
    while(line.at(0)=='#') {
        std::getline(inputFile, line, '\n');
    }
    while(!inputFile.eof()) {
        std::getline(inputFile, line, '\n');
        istringstream iss(line);
        vector<string> tokens;
        copy(istream_iterator<string>(iss),
                    istream_iterator<string>(),
                    back_inserter(tokens));
        if(tokens.size()==2) {
            int vpage_id = stoi(tokens[1]);
            instruction nextInst = {
                .inst = tokens[0],
                .vpage_id = vpage_id
            };
            instList.push_back(nextInst);
        } 
    }
    inputFile.close();
    // checkInputFile();
    // checkRFile();
    
    // initialize frame_table
    for(int i=0; i<MAX_FRAMES; i++) {
        frame_table[i].FRAME_ADDR = i;
    }
    
}
