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

    

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
