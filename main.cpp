#include <iostream>
#include <map>
#include <ctime>
#include <algorithm>
#include <vector>
#include <fstream>

//This is a CPP that will be compiled under c++ standard 11
//compilable with g++ -o main main.cpp -std=c++11

using namespace std;

struct MainInfo {
    uint8_t *mem = new uint8_t[67108864];
    int currentPID = 1024;
    int frame = 0;
} mainInfo;

struct CommandInput {
    int pageSize;
}commandInput;

struct MMUObject {
    int pageNumber;
    int frameNumber;
    int pid;
    int typeCode;//0=text/global/stack/freespace 1=char 2=short 3=int/float 4=long/double
    string name;
    int address;
    int size;
    string key;
    int physicalAddr;
    map<int, int> pageInfo; //map<pageNumber, sizeInThatPage>
};

struct PageUnit {
    int pid;
    int pageSize;
    int freeSpace;
    int pageNumber;
    int frameNumber;
    int inMem; //1 if the corresponding frame is in memory rather than RAM
};

struct FrameTable {
    map<int, PageUnit> table; //key: frameNumber, value: page struct
}frameTable;


struct MMUTable {
    map<string, MMUObject> table;
} mmuTable;

struct Process {
    int pid;
    int code; //some number 2048 - 16384 bytes
    int globals; //some number 0-1024 bytes
    const int stack{65536}; //stack constant in bytes
    //VarMap nums;
    int pages = 67108864 / commandInput.pageSize;
    int amountPageUsed = 0;
    PageUnit currentPage;
    map<int, PageUnit> pageTable;
    int totalPageRemainSpace;
};//Process struct

struct ProcessTable{
    map<int, Process*> table; //key: pid, value: process struct
}processTable;

const string COMMAND_NAME_EXIT = "exit";
const string COMMAND_NAME_CREATE = "create";
const string COMMAND_LINE_BREAK = "";

void switchMem(PageUnit page);
void takeCommand(int argc, char *argv[]);
bool isNumber(const string& s);
void createProcess();
string findFreeSpaceMMU(int size, int pid);
void printMMU();
void allocateVariable(int pid, string name, string type, int amount);
bool findExistingPID(int pid);
void terminatePID(int pid);
void createPage(Process *process);
void pageHandler(Process *process, MMUObject mmu);
void freeFromPage(Process process, MMUObject mmu);
void printPage();
bool compareEntry( std::pair<string, MMUObject>& a, std::pair<string, MMUObject>& b);
bool findExistingVariable(int pid, string name);
void freeVariable(int pid, string name);
void printProcesses();

int main(int argc, char *argv[]) {
    string input;
    srand( time( NULL ) );
    takeCommand(argc,argv);
    cout << "\nWelcome to the Memory Allocation Simulator! Using a page size of "<< commandInput.pageSize <<" bytes.\n"
            "Commands: \n"
            "* create (initializes a new process)\n"
            "  * allocate <PID> <var_name> <data_type> <number_of_elements> (allocated memory on the heap)\n"
            "  * set <PID> <var_name> <offset> <value_0> <value_1> <value_2> ... <value_N> (set the value for a variable)\n"
            "  * free <PID> <var_name> (deallocate memory on the heap that is associated with <var_name>)\n"
            "  * terminate <PID> (kill the specified process)\n"
            "  * print <object> (prints data)\n"
            "    * If <object> is \"mmu\", print the MMU memory table\n"
            "    * if <object> is \"page\", print the page table\n"
            "    * if <object> is \"processes\", print a list of PIDs for processes that are still running\n"
            "    * if <object> is a \"<PID>:<var_name>\", print the value of the variable for that process" << endl;

    //on startup create memory backing file
    if(ifstream("memfile.txt")) {
        FILE *file = fopen("memfile.txt", "rb");
        fseek(file, 0, SEEK_END);
        int leng = ftell(file);
        fclose(file);//find length of file in bytes
        if (leng == (1024*1024*488)) {
            //the file is correct
        } else {
            ofstream outputFile("memfile.txt");
            for(int i = 0; i<1024*488*1024; i++) {
                outputFile<<"0";
            }
            outputFile.close();
        }
    } else {
        ofstream outputFile("memfile.txt");
        for(int i = 0; i<1024*488*1024; i++) {
            outputFile<<"0";
        }
        outputFile.close();
    }//if doesn't already exist, create 488MB file : memfile.txt

    while(true){
        cout << ">  ";

        string input;
        getline(cin,input);
        vector<string> inpv;
        int start = 0;
        int end = input.find(" ");
        if(end != string::npos) {
            while (end != std::string::npos) {
                inpv.push_back(input.substr(start, end - start));
                start = end + 1;
                end = input.find(" ", start);
            }
            inpv.push_back(input.substr(start, input.length()));
        } else {
            inpv.push_back(input);
        }//CREATES AN INDEXIBLE INPUT VECTOR




        if(inpv[0] == COMMAND_NAME_EXIT){
            cout << "Goodbye" << endl;
            break;
        }else if (inpv[0] == COMMAND_NAME_CREATE){
            createProcess();
        }else if (inpv[0] == COMMAND_LINE_BREAK){
            //do nothing
        } else if(inpv[0] == "print"){
            if(inpv.size() != 2) {
                cout<< "print command must have mmu or page arguments"<<endl;
            } else if (inpv[1] == "mmu") {
                printMMU();
            } else if (inpv[1] == "page") {
                printPage();
            } else if(inpv[1] == "processes"){
                if(processTable.table.size()==0) {
                    cout << "There are no processes currently running" << endl;
                } else {
                    printProcesses();
                }
            } else {
                cout << "The inputted object to be printed doesn't exist" << endl;
            }//interior print conditionals

        } else if(inpv[0] == "allocate") {
            if(inpv.size() != 5) {
                cout<< "allocate requires 5 arguments"<<endl;
            } else if(!isNumber(inpv[1]) && !isNumber(inpv[4])){
                cout << "The inputted PID and amount must be an integer" << endl;
            } else {
                if(findExistingPID(stoi(inpv[1]))){
                    if(!findExistingVariable(stoi(inpv[1]),inpv[2])){
                        allocateVariable(stoi(inpv[1]),inpv[2],inpv[3],stoi(inpv[4]));
                    } else {
                        cout << "There is already a variable with that name that exists with the given PID" << endl;
                    }

                } else {
                    cout << "The provided PID has not been created yet." << endl;
                }

            }
            //terminate <PID>
        } else if (inpv[0] == "terminate") {
            if(inpv.size() != 2) {
                cout<<"terminate requires one argument "<<endl;
            } else if(isNumber(inpv[1])){
                if(findExistingPID(stoi(inpv[1]))){
                    terminatePID(stoi(inpv[1]));
                } else {
                    cout << "The provided PID has not been created yet." << endl;
                }
            } else {
                cout << "The provided PID must be an integer" << endl;
            }
        } else if(inpv[0] == "free") {
            if(isNumber(inpv[1])){
                if(findExistingVariable(stoi(inpv[1]), inpv[2])){
                    freeVariable(stoi(inpv[1]), inpv[2]);
                } else {
                    cout << "The provided PID and Variable has not been created yet." << endl;
                }
            } else {
                cout << "The provided PID must be an integer" << endl;
            }
        } else {
            cout << input << " :: invalid input" << endl;
        }

    }
    return 0;
}

//String to int checker to make sure it is valid
//Goes character by character and makes sure it is 0-9
//https://stackoverflow.com/questions/4654636/how-to-determine-if-a-string-is-a-number-with-c
bool isNumber(const string& s) {
    return !s.empty() && find_if(s.begin(),
                                 s.end(), [](char c) { return !std::isdigit(c); }) == s.end();
}

void takeCommand(int argc, char *argv[]) {
    if(argc > 1){
        if(argc < 3){
            if(isNumber(string(argv[1]))) {
                int pageHolder = stoi(string(argv[1]));
                if(pageHolder>1023 && pageHolder<32769){
                    //used link below to see if an integer is a power of 2
                    //https://stackoverflow.com/questions/108318/whats-the-simplest-way-to-test-whether-a-number-is-a-power-of-2-in-c
                    if((pageHolder & (pageHolder - 1)) == 0){
                        commandInput.pageSize = pageHolder;
                    } else {
                        cout << "The page size must be a power of two" << endl;
                        exit(0);
                    }
                } else {
                    cout << "The page size must be between 1024 and 32768" << endl;
                    exit(1);
                }
            } else {
                cout << "Your page number must be an integer" << endl;
                exit(2);
            }
        } else {
            cout << "You can only provide one argument" << endl;
            exit(4);
        }
    } else {
        cout << "You must provide a page size argument" << endl;
        exit(5);
    }
}

void createProcess(){
    Process *process = new Process;
    process->pid = mainInfo.currentPID;
    mainInfo.currentPID++;
    process->code = rand()% 14337 + 2048; //2048-16384
    process->globals= rand()% 1025; //0-1024

    MMUObject freeSpace;
    freeSpace.name = "freeSpace";
    freeSpace.pid = process->pid;
    freeSpace.address = 0;
    //arbitrary size, need 32 created threads to go over size
    //this is temporary until file is implemented
    //this number will need to increase until the collective free space
    //is filled, in the physical memory and file
    freeSpace.size = 2097152;
    freeSpace.typeCode = 0;
    freeSpace.key = to_string(freeSpace.pid) + freeSpace.name + to_string(freeSpace.address);
    mmuTable.table.insert(std::pair<string, MMUObject>(freeSpace.key,freeSpace));

    createPage(process);
    process->totalPageRemainSpace = 2097152;
    process->currentPage = process->pageTable[0];
    //assigning frame number
    if(commandInput.pageSize * mainInfo.frame >= 67108864) {
        //if the amount of frames in use is more than there are in RAM
        if(commandInput.pageSize * mainInfo.frame <= 536870912) {
            //greater than RAM but less than memory
            //switch a page to memory, and put this one on the RAM i.e. switch one on RAM to on mem, write its values

            process->currentPage.frameNumber = mainInfo.frame;
        } else {
            //TRYING TO USE MORE MEM THAN AVAILABLE
            exit(0);
        }
    } else {
        process->currentPage.frameNumber = mainInfo.frame;
    }

    mainInfo.frame++;

    MMUObject codeMMU;
    codeMMU.pid = process->pid;
    codeMMU.name = "<TEXT>";
    codeMMU.size = process->code;
    codeMMU.typeCode = 0;
    codeMMU.key = to_string(codeMMU.pid) + codeMMU.name;
    string freeSpaceMMUKey = findFreeSpaceMMU(codeMMU.size, codeMMU.pid);
    codeMMU.address = mmuTable.table.at(freeSpaceMMUKey).address;
    mmuTable.table.at(freeSpaceMMUKey).address = codeMMU.address+codeMMU.size;
    mmuTable.table.at(freeSpaceMMUKey).size = mmuTable.table.at(freeSpaceMMUKey).size - codeMMU.size;

    mmuTable.table.insert(std::pair<string, MMUObject>(codeMMU.key,codeMMU));

    pageHandler(process,codeMMU);

    MMUObject globalMMU;
    globalMMU.pid = process->pid;
    globalMMU.name = "<GLOBALS>";
    globalMMU.size = process->globals;
    globalMMU.typeCode = 0;
    globalMMU.key = to_string(globalMMU.pid) + globalMMU.name;
    freeSpaceMMUKey = findFreeSpaceMMU(globalMMU.size, globalMMU.pid);
    globalMMU.address = mmuTable.table.at(freeSpaceMMUKey).address;
    mmuTable.table.at(freeSpaceMMUKey).address = globalMMU.address+globalMMU.size;
    mmuTable.table.at(freeSpaceMMUKey).size = mmuTable.table.at(freeSpaceMMUKey).size - globalMMU.size;

    mmuTable.table.insert(std::pair<string, MMUObject>(globalMMU.key,globalMMU));

    pageHandler(process,globalMMU);

    MMUObject stackMMU;
    stackMMU.pid = process->pid;
    stackMMU.name = "<STACK>";
    stackMMU.size = process->stack;
    stackMMU.typeCode = 0;
    stackMMU.key = to_string(stackMMU.pid) + stackMMU.name;
    freeSpaceMMUKey = findFreeSpaceMMU(stackMMU.size, stackMMU.pid);
    stackMMU.address = mmuTable.table.at(freeSpaceMMUKey).address;
    mmuTable.table.at(freeSpaceMMUKey).address = stackMMU.address+stackMMU.size;
    mmuTable.table.at(freeSpaceMMUKey).size = mmuTable.table.at(freeSpaceMMUKey).size - stackMMU.size;

    mmuTable.table.insert(std::pair<string, MMUObject>(stackMMU.key,stackMMU));

    pageHandler(process,stackMMU);

    processTable.table[process->pid] = process;
    cout << process->pid << endl;
}

void allocateVariable(int pid, string name, string type, int amount) {
    MMUObject stackMMU;
    stackMMU.pid = pid;
    stackMMU.name = name;
    if(type == "char"){
        stackMMU.size = amount;
        stackMMU.typeCode = 1;
    } else if(type == "short") {
        stackMMU.size = amount*2;
        stackMMU.typeCode = 2;
    } else if(type == "int" || type == "floats") {
        stackMMU.size = amount*4;
        stackMMU.typeCode = 3;
    } else {
        stackMMU.size = amount*8;
        stackMMU.typeCode = 4;
    }


    stackMMU.key = to_string(stackMMU.pid) + stackMMU.name;
    string freeSpaceMMUKey = findFreeSpaceMMU(stackMMU.size, stackMMU.pid);
    stackMMU.address = mmuTable.table.at(freeSpaceMMUKey).address;
    mmuTable.table.at(freeSpaceMMUKey).address = stackMMU.address+stackMMU.size;
    mmuTable.table.at(freeSpaceMMUKey).size = mmuTable.table.at(freeSpaceMMUKey).size - stackMMU.size;
    if( mmuTable.table.at(freeSpaceMMUKey).size == 0 ) {
        mmuTable.table.erase(freeSpaceMMUKey);
    }

    mmuTable.table.insert(std::pair<string, MMUObject>(stackMMU.key,stackMMU));

    Process *currentProcess = processTable.table[pid];
    pageHandler(currentProcess,stackMMU);
    processTable.table[currentProcess->pid] = currentProcess;
}

string findFreeSpaceMMU(int size, int pid) {
    //found a map iterator
    //https://stackoverflow.com/questions/26281979/c-loop-through-map
    int lowest = -1;
    string ret;
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name=="freeSpace" && loc.second.size>=size && pid == loc.second.pid) {
            lowest = loc.second.address;
            ret = loc.second.key;
        }
    }
    if(lowest == -1){
        return "N/A";
    }
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name=="freeSpace" && loc.second.size>=size && pid == loc.second.pid && loc.second.address < lowest) {
            lowest = loc.second.address;
            ret = loc.second.key;
        }
    }
    return ret;

}

void printMMU() {
    printf("|%4s  | %13s | %12s | %4s \n", "PID", "Variable Name", "Virtual Addr", "Size");
    printf("+------+---------------+--------------+------------\n");
    //Looked up how to change map to vector, for sorting purposes
    //https://stackoverflow.com/questions/5056645/sorting-stdmap-using-value
    vector<std::pair<string, MMUObject>> pairs;
    for (auto itr = mmuTable.table.begin(); itr != mmuTable.table.end(); ++itr) {
        pairs.push_back(*itr);
    }
    //found sorting method to compare two attributes here
    //https://stackoverflow.com/questions/6771374/sorting-an-stl-vector-on-two-values
    sort( pairs.begin(), pairs.end(), compareEntry );
    for(int i=0; i<pairs.size(); i++) {
        if(pairs[i].second.name!="freeSpace") {
            printf("| %4d | %13s | %12s | %10d \n", pairs[i].second.pid, pairs[i].second.name.c_str(), to_string(pairs[i].second.address).c_str(), pairs[i].second.size);
        }
    }

}

bool compareEntry( std::pair<string, MMUObject>& a, std::pair<string, MMUObject>& b) {
    if( a.second.pid != b.second.pid)
        return (a.second.pid < b.second.pid);
    return (a.second.address < b.second.address);
}

bool findExistingPID(int pid){
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name!="freeSpace" && loc.second.pid==pid) {
            return true;
        }
    }
    return false;
}

bool findExistingVariable(int pid, string name) {
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name!="freeSpace" && loc.second.pid==pid && loc.second.name == name) {
            return true;
        }
    }
    return false;
}

void terminatePID(int pid){
    //different type of loop to go trough all entries of map, this is necessary
    //because the old loop
    //https://stackoverflow.com/questions/8234779/how-to-remove-from-a-map-while-iterating-it
    for (auto it = mmuTable.table.begin(); it != mmuTable.table.end(); ) {
        if (it->second.pid == pid) {
            it = mmuTable.table.erase(it);
        } else {
            ++it;
        }
    }
}

void freeVariable(int pid, string name) {
    MMUObject freeSpace;
    freeSpace.name = "freeSpace";
    freeSpace.pid = pid;
    freeSpace.address = mmuTable.table.at(to_string(pid)+name).address;
    //arbitraury size, need 32 created threads to go over size
    //this is temporary until file is implemented
    //this number will need to increase until the collective free space
    //is filled, in the physical memory and file
    freeSpace.size = mmuTable.table.at(to_string(pid)+name).size;
    freeSpace.typeCode = 0;
    freeSpace.key = to_string(pid) + freeSpace.name + to_string(freeSpace.address);
    mmuTable.table.erase(to_string(pid)+name);
    for (auto it = mmuTable.table.begin(); it != mmuTable.table.end(); ) {
        //loc.first string (key)
        //loc.second string's value
        if(it->second.name=="freeSpace" && freeSpace.pid == it->second.pid) {
            if((it->second.address + it->second.size) == freeSpace.address){
                freeSpace.size += it->second.size;
                freeSpace.address = it->second.address;
                it = mmuTable.table.erase(it);
            } else if((freeSpace.address + freeSpace.size) == it->second.address){
                freeSpace.size += it->second.size;
                it = mmuTable.table.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
    mmuTable.table.insert(std::pair<string, MMUObject>(freeSpace.key,freeSpace));
}

void createPage(Process *process){
    //new process with no page
    while(process->amountPageUsed < process->pages) {
        PageUnit page;
        page.pid = process->pid;
        page.pageSize = commandInput.pageSize;
        page.freeSpace = page.pageSize;
        page.pageNumber = process->amountPageUsed;
        process->amountPageUsed++;
        page.frameNumber = -1; //marked for empty page
        process->pageTable[page.pageNumber] = page;
    }
}

void pageHandler(Process *process, MMUObject mmu){
    int remainData = mmu.size;
    int dataStored = 0;
    //check if there is enough space in all pages
    if(remainData > process->totalPageRemainSpace){
        cout << "no more space in page" << endl;
        return ;
    }

    while(process->currentPage.freeSpace == 0){
        process->currentPage = process->pageTable[(process->currentPage.pageNumber+1)%process->pages];
    }

    //the free space start from freeAddr
    int freeAddr = commandInput.pageSize - process->currentPage.freeSpace;

    mmu.pageNumber = process->currentPage.pageNumber;
    mmu.frameNumber = process->currentPage.frameNumber;
    mmu.physicalAddr = mmu.frameNumber * commandInput.pageSize + (freeAddr);

    while(remainData > 0){
        //mmu pageInfo change here,
        //for one mmu, the pageInfo map tells us how much data we stored in which page
        if(remainData < process->currentPage.freeSpace){
            //if the remainData is smaller, the page could handle the data and store data
            //it means the while loop will stop
            mmu.pageInfo[process->currentPage.pageNumber] = remainData;
        }else{
            //if the freespace is smaller, the page could not store that much data,
            //it will only store whatever amount of free space it owns, and keep running the loop
            mmu.pageInfo[process->currentPage.pageNumber] = process->currentPage.freeSpace;
        }

        remainData -= process->currentPage.freeSpace;

        if(remainData < 0){
            process->totalPageRemainSpace -= remainData *(-1);
            process->currentPage.freeSpace = remainData *(-1);
            frameTable.table[process->currentPage.frameNumber] = process->currentPage;
            process->pageTable[process->currentPage.pageNumber] = process->currentPage;
        }else{
            process->totalPageRemainSpace -= process->currentPage.freeSpace;
            process->currentPage.freeSpace = 0;
            frameTable.table[process->currentPage.frameNumber] = process->currentPage;
            process->pageTable[process->currentPage.pageNumber] = process->currentPage;
            //move to next page
            //check if there is enough space in all pages
            while(process->currentPage.freeSpace == 0) {
                process->currentPage = process->pageTable[(process->currentPage.pageNumber + 1) % process->pages];
            }
            //frameNumber need to be fixed
            process->currentPage.frameNumber = mainInfo.frame;
            mainInfo.frame++;
        }
    }

    //update the mmu in the mmuTable
    mmuTable.table[mmu.key] = mmu;
}

void freeFromPage(Process process, MMUObject mmu){

}

void switchMem(PageUnit* page) {
    //method to switch input process out of memory, and another process into memory
    int swp = 0; //switched page accomplished
    page->inMem = 0;
    for (auto& processLoc : processTable.table) {
        if (swp == 0) {
            for (auto& pageLoc : processLoc.second->pageTable) {
                if (pageLoc.second.inMem == 0) {
                    //write into "mem" starting at the index that the current page was taken out of (e.g. frame * size)
                    pageLoc.second.inMem = 1;
                    break;
                }
            }
        }
    }

} // switches input page onto RAM, writes first page already on RAM onto file, switches tag back

void printPage(){
    printf("|%4s  | %11s | %12s \n", "PID", "Page Number", "Frame Number");
    printf("+------+-------------+--------------\n");
    for (auto const& processLoc : processTable.table) {
        //go through processTable
        for (auto const& pageLoc : processLoc.second->pageTable) {
            //go through pageTable in every process
            if (pageLoc.second.frameNumber != -1) {
                printf("| %4d | %11d | %12d  \n", processLoc.second->pid, pageLoc.second.pageNumber,
                       pageLoc.second.frameNumber);
            }
        }
    }
}

void printProcesses() {
    for (auto const& processLoc : processTable.table) {
        cout << processLoc.second->pid << endl;
    }
}
