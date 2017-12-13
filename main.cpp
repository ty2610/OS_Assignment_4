#include <iostream>
#include <map>
#include <ctime>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <string.h>

//This is a CPP that will be compiled under c++ standard 11
//compilable with g++ -o main main.cpp -std=c++11

using namespace std;

struct MainInfo {
    uint8_t *mem = new uint8_t[67108864];
    int currentPID = 1024;
    vector<int> frame;
} mainInfo;

struct CommandInput {
    int pageSize;
}commandInput;

struct MMUObject {
    int pageNumber;
    int frameNumber;
    bool set;
    int pid;
    int typeCode;//0=text/global/stack/freespace 1=char 2=short 3=int 4=double 5=long 6=float
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

struct VariableObject {
    int typeCode;//0=text/global/stack/freespace 1=char 2=short 3=int 4=double 5=long 6=float
    char charValue;
    short shortValue;
    int intValue;
    double doubleValue;
    long longValue;
    float floatValue;
};

const string COMMAND_NAME_EXIT = "exit";
const string COMMAND_NAME_CREATE = "create";
const string COMMAND_LINE_BREAK = "";

void switchMem(PageUnit* page, int fnumber);
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
void freeFromPage(Process *process, MMUObject mmu);
void printPage();
bool compareEntry( std::pair<string, MMUObject>& a, std::pair<string, MMUObject>& b);
bool findExistingVariable(int pid, string name);
void freeVariable(int pid, string name);
void printProcesses();
int findExistingVariableType(int pid, string name);
void setValues(int pid, string name, int offset, vector<VariableObject> values);
int lowestFrameNum();
void printVariable(int pid, string name);
string trimWhiteSpace(string str);

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
        cout<< "CREATING NEW 488MB FILE, INITIALIZED TO 0s"<<endl;
        for(int i = 0; i<1024*488*1024; i++) {
            outputFile<<"0";
        }
        outputFile.close();
    }//if doesn't already exist, create 488MB file : memfile.txt

    mainInfo.frame.push_back(0);

    while(true){
        restart:
        cout << ">  ";

        string input;
        getline(cin,input);
        input = trimWhiteSpace(input);
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
            if(inpv.size() < 2 || inpv.size() > 3) {
                cout<< "print command must have 2 or 3 mmu or page arguments"<<endl;
            } else if (inpv[1] == "mmu" && inpv.size() == 2) {
                printMMU();
            } else if (inpv[1] == "page" && inpv.size() == 2) {
                printPage();
            } else if(inpv[1] == "processes" && inpv.size() == 2){
                if(processTable.table.size()==0) {
                    cout << "There are no processes currently running" << endl;
                } else {
                    printProcesses();
                }
            } else if(isNumber(inpv[1])) {
                if(findExistingVariable(stoi(inpv[1]),inpv[2])){
                    if(mmuTable.table.at(inpv[1]+inpv[2]).set){
                        printVariable(stoi(inpv[1]),inpv[2]);
                    } else {
                        cout << "The pid and variable combination has not had a value set yet" << endl;
                    }

                } else {
                    cout << "The provided pid and name combination doesn't exist" << endl;
                }
            } else {
                cout << "The inputted object to be printed doesn't exist" << endl;
            }
        } else if(inpv[0] == "allocate") {
            if(inpv.size() != 5) {
                cout<< "allocate requires 5 arguments"<<endl;
            } else if(!isNumber(inpv[1]) && !isNumber(inpv[4])){
                cout << "The inputted PID and amount must be an integer" << endl;
            } else {
                if(stoi(inpv[4])>0) {
                    if(findExistingPID(stoi(inpv[1]))){
                        if(!findExistingVariable(stoi(inpv[1]),inpv[2])){
                            allocateVariable(stoi(inpv[1]),inpv[2],inpv[3],stoi(inpv[4]));
                        } else {
                            cout << "There is already a variable with that name that exists with the given PID" << endl;
                        }
                    } else {
                        cout << "The provided PID has not been created yet." << endl;
                    }
                } else {
                    cout << "You must allocate more than 0" << endl;
                }

            }
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
        } else if(inpv[0] == "set") {
            if(inpv.size() > 4){
                if(isNumber(inpv[1]) && isNumber(inpv[3])){
                    if(findExistingVariable(stoi(inpv[1]), inpv[2])){
                        //setValues(int pid, string name, int offset, vector<T> values)
                        int variableType = findExistingVariableType(stoi(inpv[1]), inpv[2]);
                        vector<VariableObject> heldValues;
                        for(int i=4; i<inpv.size(); i++) {
                            if(variableType == 1 && inpv[i].length()>1) {
                                cout << "The provided char argument has more than one char" << endl;
                                //needed to get to top of while loop
                                //https://stackoverflow.com/questions/245742/examples-of-good-gotos-in-c-or-c
                                goto restart;
                            } else if(variableType == 2 && !isNumber(inpv[i])){
                                cout << "The provided short must be a number" << endl;
                                goto restart;
                            } else if(variableType == 2) {
                                //short error checking
                                //https://stackoverflow.com/questions/5834439/validate-string-within-short-range
                                istringstream istr(inpv[i]);
                                short s;
                                if ((istr >> s) and istr.eof()){
                                } else {
                                    cout << "the provided short is not a correct short" << endl;
                                    goto restart;
                                }
                            } else if(variableType == 3 && !isNumber(inpv[i])){
                                cout << "The provided int must be a number" << endl;
                                goto restart;
                            } else if(variableType == 3) {
                                try{
                                    stoi(inpv[i]);
                                } catch(exception e) {
                                    cout << "The provided int is incorrect" << endl;
                                    goto restart;
                                }
                            }else if(variableType == 4) {
                                //helped with coming up with double checker
                                //https://stackoverflow.com/questions/29169153/how-do-i-verify-a-string-is-valid-double-even-if-it-has-a-point-in-it
                                try {
                                    stod(inpv[i]);
                                } catch (exception e){
                                    cout << "The provided double must be a valid double" << endl;
                                    goto restart;
                                }
                            } else if (variableType == 5) {
                                try {
                                    stol(inpv[i]);
                                } catch (exception e){
                                    cout << "The provided long must be a valid long" << endl;
                                    goto restart;
                                }
                            } else if(variableType == 6 ) {
                                try {
                                    stof(inpv[i]);
                                } catch (exception e){
                                    cout << "The provided float must is incorrectly formatted" << endl;
                                    goto restart;
                                }
                            }
                            VariableObject variableObject;
                            variableObject.typeCode = variableType;
                            //0=text/global/stack/freespace 1=char 2=short 3=int 4=double 5=long 6=float
                            //looked up switch syntax, can never remember it
                            //http://en.cppreference.com/w/cpp/language/switch
                            switch(variableType){
                                case 1 : variableObject.charValue = inpv[i].at(0);
                                    break;
                                case 2 : variableObject.shortValue = stoi(inpv[i]);
                                    break;
                                case 3 : variableObject.intValue = stoi(inpv[i]);
                                    break;
                                case 4 : variableObject.doubleValue = stod(inpv[i]);
                                    break;
                                case 5 : variableObject.longValue = stol(inpv[i]);
                                    break;
                                case 6 : variableObject.floatValue = stof(inpv[i]);
                                    break;
                            }
                            heldValues.push_back(variableObject);
                        }
                        int totalUsedBytes = 0;
                        int offset = stoi(inpv[3]);
                        switch(variableType){
                            case 1 : totalUsedBytes = heldValues.size();
                                break;
                            case 2 : totalUsedBytes = heldValues.size() * 2;
                                offset = offset * 2;
                                break;
                            case 3 : totalUsedBytes = heldValues.size() * 4;
                                offset = offset * 4;
                                break;
                            case 4 : totalUsedBytes = heldValues.size() * 8;
                                offset = offset * 8;
                                break;
                            case 5 : totalUsedBytes = heldValues.size() * 8;
                                offset = offset * 8;
                                break;
                            case 6 : totalUsedBytes = heldValues.size() * 4;
                                offset = offset * 4;
                                break;
                        }
                        totalUsedBytes += offset;
                        if(mmuTable.table.at(inpv[1]+ inpv[2]).size < totalUsedBytes){
                            cout << "The set function goes past the allotted space created for the variable" << endl;
                            goto restart;
                        }
                        setValues(stoi(inpv[1]), inpv[2], offset, heldValues);
                    } else {
                        cout << "The provided PID and Variable has not been created yet." << endl;
                    }
                } else {
                    cout << "The given PID and offset must be numbers" << endl;
                }
            } else {
                cout << "There must be at least 4 arguments for the set command" << endl;
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
    freeSpace.size = 2097152;
    freeSpace.typeCode = 0;
    freeSpace.key = to_string(freeSpace.pid) + freeSpace.name + to_string(freeSpace.address);
    mmuTable.table.insert(std::pair<string, MMUObject>(freeSpace.key,freeSpace));


    //pages and their frames should NOT be initialized UNLESS they're getting data

    createPage(process);
    process->totalPageRemainSpace = 2097152;
    process->currentPage = process->pageTable[0];
    process->currentPage.frameNumber = lowestFrameNum();
    int framesUsed = mainInfo.frame[0]-(mainInfo.frame.size()-1);
    //assigning frame number
    if(commandInput.pageSize * process->currentPage.frameNumber >= 67108864) {
        //if the amount of frames in use is more than there are in RAM
        if(commandInput.pageSize * framesUsed <= 536870912){
            //this should actually only happen if the frame in question is in memory
            switchMem(&(process->currentPage),process->currentPage.frameNumber);
        } else {
            //TRYING TO USE MORE MEM THAN AVAILABLE
            exit(0);
        }
    }

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
    } else if(type == "int") {
        stackMMU.size = amount*4;
        stackMMU.typeCode = 3;
    } else if(type == "double") {
        stackMMU.size = amount*8;
        stackMMU.typeCode = 4;
    } else if(type == "long") {
        stackMMU.size = amount*8;
        stackMMU.typeCode = 5;
    } else {
        stackMMU.size = amount*4;
        stackMMU.typeCode = 6;
    }
    stackMMU.set = false;

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
    printf("|%4s  | %13s | %11s | %4s \n", "PID", "Variable Name", "Virtual Addr", "Size");
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
            printf("| %4d | %13s | 0x%08x | %10d \n", pairs[i].second.pid, pairs[i].second.name.c_str(), pairs[i].second.address, pairs[i].second.size);
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

int findExistingVariableType(int pid, string name) {
    for (auto const& loc : mmuTable.table)
    {
        //loc.first string (key)
        //loc.second string's value
        if(loc.second.name!="freeSpace" && loc.second.pid==pid && loc.second.name == name) {
            return loc.second.typeCode;
        }
    }
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

    //remove from process map, if the pid does not exist in map
    //this line will have no effect
    processTable.table.erase(pid);

    //remove from frameTable and push back the free frameNumber
    for(auto const& loc : frameTable.table){
        if(loc.second.pid == pid){
            mainInfo.frame.push_back(loc.first);
            frameTable.table.erase(loc.first);
        }
    }
}

void freeVariable(int pid, string name) {
    MMUObject mmu = mmuTable.table[to_string(pid)+name];
    MMUObject freeSpace;
    freeSpace.name = "freeSpace";
    freeSpace.pid = pid;
    freeSpace.address = mmuTable.table.at(to_string(pid)+name).address;
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

    Process *process = processTable.table[pid];
    freeFromPage(process,mmu);
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
            process->currentPage.frameNumber = lowestFrameNum();
            int framesUsed = mainInfo.frame[0]-(mainInfo.frame.size()-1);
            if(commandInput.pageSize * framesUsed >= 67108864) {
                if(commandInput.pageSize * framesUsed <= 536870912) {

                    switchMem(&(process->currentPage), process->currentPage.frameNumber);

                } else {
                    //TRYING TO USE MORE MEM THAN AVAILABLE
                    exit(0);
                }
            }
        }
    }

    //update the mmu in the mmuTable
    mmuTable.table[mmu.key] = mmu;
}

void freeFromPage(Process *process, MMUObject mmu){

    int pageNum;
    PageUnit page;
    int sizeIn;
    //Adding back for both space.

    for(auto const& loc : mmu.pageInfo){
        pageNum = loc.first;
        sizeIn = loc.second;

        page = process->pageTable[pageNum];
        page.freeSpace += sizeIn;
        process->totalPageRemainSpace += sizeIn;

        //if the page is empty after freeing, remove from frameTable
        if(page.freeSpace == page.pageSize){
            int frameNumber = page.frameNumber;
            frameTable.table.erase(frameNumber);
            page.frameNumber = -1; // means the page is empty and removed from the frameTable
            mainInfo.frame.push_back(frameNumber);
        }

        process->pageTable[pageNum] = page;
    }

    processTable.table[process->pid] = process;

}

void switchMem(PageUnit* page, int fnumber) {
    int swp = 0; 
    fstream fmem;
    fmem.open("memfile.txt");
    long pos = fmem.tellp() + long(fnumber*commandInput.pageSize);
    page->inMem = 0;
    for (auto& processLoc : processTable.table) {
        if (swp == 0) {
            for (auto& pageLoc : processLoc.second->pageTable) {
                if (pageLoc.second.inMem == 0) {
                    vector<std::pair<string, MMUObject>> pairs;
                    for (auto itr = mmuTable.table.begin(); itr != mmuTable.table.end(); ++itr) {
                        pairs.push_back(*itr);
                    }
                    //excerpted code from MMUprint
                    sort( pairs.begin(), pairs.end(), compareEntry );
                    for(int i=0; i<pairs.size(); i++) {
                        fmem.seekp(pos);
                        if(pairs[i].second.pid ==pageLoc.second.pid) {
                            fmem<<pairs[i].second.address;
                        }
                    }
                    fmem.close();
                    //write into "mem" starting at the index that the current page was taken out of (e.g. frame * size)
                    page->frameNumber = pageLoc.second.frameNumber;
                    pageLoc.second.frameNumber = fnumber;
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
                if(pageLoc.second.inMem != 1) {
                    printf("\x1b[31m" "| %4d | %11d | %12d  \n" "\x1b[0m", processLoc.second->pid, pageLoc.second.pageNumber,
                           pageLoc.second.frameNumber);
                }else {
                    printf("| %4d | %11d | %12d  \n", processLoc.second->pid, pageLoc.second.pageNumber,
                           pageLoc.second.frameNumber);
                }
            }
        }
    }
}

void printProcesses() {
    for (auto const& processLoc : processTable.table) {
        cout << processLoc.second->pid << endl;
    }
}

void setValues(int pid, string name, int offset, vector<VariableObject> values) {
    char *charPointer;
    short *shortPointer;
    int *intPointer;
    double *doublePointer;
    long *longPointer;
    float *floatPointer;
    uint8_t *mem = mainInfo.mem;
    MMUObject setMMUObject = mmuTable.table.at(to_string(pid)+name);
    mmuTable.table.at(to_string(pid)+name).set = true;
    int location = setMMUObject.physicalAddr + offset;
    switch(values.at(0).typeCode){
        case 1 : charPointer = (char*) (mem+location);
            break;
        case 2 : shortPointer = (short*) (mem+location);
            break;
        case 3 : intPointer = (int*) (mem+location);
            break;
        case 4 : doublePointer = (double*) (mem+location);
            break;
        case 5 : longPointer = (long*) (mem+location);
            break;
        case 6 : floatPointer = (float*) (mem+location);
            break;
    }
    for(int i=0; i<values.size(); i++){
        switch(values.at(0).typeCode){
            case 1 : charPointer[i] = values.at(i).charValue;
                break;
            case 2 : shortPointer[i*2] = values.at(i).shortValue;
                break;
            case 3 : intPointer[i*4] = values.at(i).intValue;
                break;
            case 4 : doublePointer[i*8] = values.at(i).doubleValue;
                break;
            case 5 : longPointer[i*8] = values.at(i).longValue;
                break;
            case 6 : floatPointer[i*4] = values.at(i).floatValue;
                break;
        }
    }
}

int lowestFrameNum(){
    //the vector[0] is the main number to get the new frame number
    //the rest numbers are the numbers which were used before, then put back to the vector
    int min = mainInfo.frame[0];
    int loc = 0;
    for(int i=0;i<mainInfo.frame.size();i++){
        if(mainInfo.frame[i] < min){
            min = mainInfo.frame[i];
            loc = i;
        }
    }
    if(min == mainInfo.frame[0]){
        mainInfo.frame[0] += 1;
    }else{
        mainInfo.frame.erase(mainInfo.frame.begin()+loc);
    }
    return min;
}

void printVariable(int pid, string name) {
    MMUObject variableMMUObject = mmuTable.table.at(to_string(pid)+name);
    //0=text/global/stack/freespace 1=char 2=short 3=int 4=double 5=long 6=float
    char *charPointer;
    short *shortPointer;
    int *intPointer;
    double *doublePointer;
    long *longPointer;
    float *floatPointer;
    int amount = 0;
    uint8_t *mem = mainInfo.mem;
    switch(variableMMUObject.typeCode){
        case 1 : charPointer = (char*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size;
            break;
        case 2 : shortPointer = (short*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size/2;
            break;
        case 3 : intPointer = (int*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size/4;
            break;
        case 4 : doublePointer = (double*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size/8;
            break;
        case 5 : longPointer = (long*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size/8;
            break;
        case 6 : floatPointer = (float*) (mem+variableMMUObject.physicalAddr);
            amount = variableMMUObject.size/4;
            break;
    }

    for(int i=0; i<amount; i++){
        if(i==4){
            cout << "... " << "[" << amount << " items]";
            goto endOfPrint;
        }
        switch(variableMMUObject.typeCode){
            case 1 : cout << charPointer[i];
                break;
            case 2 : cout << shortPointer[i*2];
                break;
            case 3 : cout << intPointer[i*4];
                break;
            case 4 : cout << doublePointer[i*8];
                break;
            case 5 : cout << longPointer[i*8];
                break;
            case 6 : cout << floatPointer[i*4];
                break;
        }
        if(amount-1!=1){
            cout << ", ";
        }

    }
    endOfPrint:
    cout << endl;
}

//needed whitespace trimmer
//https://stackoverflow.com/questions/25829143/trim-whitespace-from-a-string
string trimWhiteSpace(string str) {
    size_t first = str.find_first_not_of(' ');
    if (string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}
