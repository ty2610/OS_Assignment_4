#include <iostream>
#include <map>



using namespace std;

struct VarMap {
    map<string, char> cmap;
    map<string, short> smap;
    map<string, int> imap;
    map<string, float> fmap;
};//map struct

struct Process {
    int pid;
    int code; //some number 2048 - 16384 bytes
    int globals; //some number 0-1024 bytes
    const int stack{65536}; //stack constant in bytes
    VarMap nums;    
};//Process struct

const string COMMAND_NAME_EXIT = "exit";
const string COMMAND_NAME_CREATE = "create";
const string COMMAND_LINE_BREAK = "";

int main() {
    string input;
    int id = 1024;

    cout << "\nWelcome to the Memory Allocation Simulator! Using a page size of 8192 bytes.\n"
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
        cout << "> ";

        getline(cin,input);

        if(input == COMMAND_NAME_EXIT){
            break;
        }else if (input == COMMAND_NAME_CREATE){
            Process *process = new Process;
            process->pid = id++;
            process->code = rand()% 14337 + 2048; //2048-16384
            process->globals= rand()% 1024; //0-1024

            cout << process->pid << endl;
            //cout << process->code << endl;
            //cout << process->globals << endl;
            //cout << process->stack << endl;
        }else if (input == COMMAND_LINE_BREAK){
          //do nothing
        } else{
            cout << input << " :: invalid input" << endl;
        }

    }
    return 0;
}
