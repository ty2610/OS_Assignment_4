#include <iostream>
#include <map>
#include <ctime>
#include <algorithm>

//This is a CPP that will be compiled under c++ standard 11
//compilable with g++ -o main main.cpp -std=c++11

using namespace std;

struct MainInfo {
    uint8_t *mem = new uint8_t[67108864];
    int currentPID = 1024;
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
};

struct FrameTable {

};

struct MMUTable {
    map<string, MMUObject> table;
} mmuTable;

struct Process {
    int pid;
    int code; //some number 2048 - 16384 bytes
    int globals; //some number 0-1024 bytes
    const int stack{65536}; //stack constant in bytes
    //VarMap nums;
};//Process struct

const string COMMAND_NAME_EXIT = "exit";
const string COMMAND_NAME_CREATE = "create";
const string COMMAND_LINE_BREAK = "";

void takeCommand(int argc, char *argv[]);
bool isNumber(const string& s);
void createProcess();
string findFreeSpaceMMU(int size, int pid);
void printMMU();
void allocateVariable(int pid, string name, string type, int amount);
bool findExistingPID(int pid);
void terminatePID(int pid);

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
        } else if(inpv[0] == "print mmu"){
            //conditional statements for print
            printMMU();

        } else if(inpv[0] == "allocate") {
            //allocateVariable(int pid, string name, string type, int amount)
            //allocate 1024 point int 1
            if(!isNumber(inpv[1]) && !isNumber(inpv[4])){
                cout << "The inputted PID and amount must be an integer" << endl;
            } else {
                if(findExistingPID(stoi(inpv[1]))){
                    allocateVariable(stoi(inpv[1]),inpv[2],inpv[3],stoi(inpv[4]));
                } else {
                    cout << "The provided PID has not been created yet." << endl;
                }

            }
        //terminate <PID>
        } else if (inpv[0] == "terminate") {
            if(isNumber(inpv[1])){
                if(findExistingPID(stoi(input.substr(10,4)))){
                    terminatePID(stoi(input.substr(10,4)));
                } else {
                    cout << "The provided PID has not been created yet." << endl;
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
    //arbitraury size, need 32 created threads to go over size
    //this is temporary until file is implemented
    //this number will need to increase until the collective free space
    //is filled, in the physical memory and file
    freeSpace.size = 2097152;
    freeSpace.typeCode = 0;
    freeSpace.key = to_string(freeSpace.pid) + freeSpace.name;
    mmuTable.table.insert(std::pair<string, MMUObject>(freeSpace.key,freeSpace));

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
}

string findFreeSpaceMMU(int size, int pid) {
    //found a map iterator
    //https://stackoverflow.com/questions/26281979/c-loop-through-map
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name=="freeSpace" && loc.second.size>=size && pid == loc.second.pid) {
            return loc.first;
        }
    }
}

void printMMU() {
    printf("|%4s  | %13s | %12s | %4s \n", "PID", "Variable Name", "Virtual Addr", "Size");
    printf("+------+---------------+--------------+------------\n");
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name!="freeSpace") {
            printf("| %4d | %13s | %12s | %10d \n", loc.second.pid, loc.second.name.c_str(), to_string(loc.second.address).c_str(), loc.second.size);
        }
    }
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
