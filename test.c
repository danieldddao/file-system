#include <stdio.h>
#include <dirent.h>


int main(int argc, char **argv) {
    
    char a[] = "abcd";
    char *first = a;
    char *second = first;
    printf("%d %d\n",(int)&a, second);
    return 0;
}



