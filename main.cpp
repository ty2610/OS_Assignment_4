#include <iostream>
#include <map>
#include <ctime>
#include <algorithm>

//This is a CPP that will be compiled under c++ standard 11
//compilable with g++ -o main main.cpp -std=c++11

using namespace std;

struct VarMap {
    map<string, char> cmap;
    map<string, short> smap;
    map<string, int> imap;
    map<string, float> fmap;
};//map struct

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
    string address;
    int size;
    string key;
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
    bool active; //if the process has been terminated or not
};//Process struct

const string COMMAND_NAME_EXIT = "exit";
const string COMMAND_NAME_CREATE = "create";
const string COMMAND_LINE_BREAK = "";

void takeCommand(int argc, char *argv[]);
bool isNumber(const string& s);
void createProcess();

int main(int argc, char *argv[]) {
    string input;
    int id = 1023;
    MMUObject freeSpace;
    freeSpace.name = "freeSpace";
    freeSpace.pid = 0;
    freeSpace.address = 0;
    freeSpace.size = 67108864;
    freeSpace.typeCode = 0;
    freeSpace.key = freeSpace.name + to_string(freeSpace.pid);
    //THIS INSERT IS NOT TESTED
    mmuTable.table.insert(std::pair<string, MMUObject>(freeSpace.key,freeSpace));
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

        getline(cin,input);

        if(input == COMMAND_NAME_EXIT){
            cout << "Goodbye" << endl;
            break;
        }else if (input == COMMAND_NAME_CREATE){
            createProcess();
        }else if (input == COMMAND_LINE_BREAK){
          //do nothing
        } else{
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
    process->active = true;


    cout << process->pid << endl;
}
