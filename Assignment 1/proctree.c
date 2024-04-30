#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>

#define MAXLEN 100

int main(int argc, char* argv[]){
    int indent = 0;
    if(argc==1){ //if no node name is given
        printf("Run with a node name\n");
        exit(0);
    }
    if(argc > 2) indent = atoi(argv[2]); //indentation value (in tabs) given as command line argument

    FILE* fptr = NULL;
    fptr = fopen("treeinfo.txt", "r"); //opening the text file
    if(fptr==NULL){
        printf("ERROR: Couldn't open the text file successfully!\n");
        exit(0);
    }

    char *node = argv[1]; //current node name
    char line[MAXLEN]; //to store each line from the text file
    int found = 0;
    while(fgets(line, MAXLEN, fptr)){ //reading each line from the text file
        int i=0;
        while(i<strlen(node) && line[i]==node[i]){
            i++;
        } 
        if(!(i==strlen(node) && line[i]==' '))continue;
        found = 1; //required node match found!

        for(int j=0; j<indent; j++){
            printf("\t"); //to give the required indentation
        }
        printf("%s (%u)\n", node, getpid()); //printing the current node

        i++;
        char num[MAXLEN];
        int j=0;
        while(line[i]>='0' && line[i]<='9'){
            num[j++]=line[i++];
        }
        num[j]='\0';
        int num_child = atoi(num); //number of children of the current node
        
        for(int j=0; j<num_child; j++){ //for each child node...
            char new_node[MAXLEN]; //to store the new child node name
            i++;
            int id = 0;
            while(line[i]!=' ' && line[i]!='\n' && line[i]!='\0'){
                new_node[id++]=line[i++];
            }
            new_node[id]='\0'; 

            pid_t pid = fork(); //to create a new child process (for child nodes' traversal)

            if(pid<0){
                printf("ERROR: New child process creation failed!\n");
                exit(0);
            }
            if(pid==0){
                //in the child process...
                char new_indent_val[MAXLEN]; //to store the new indentation value (in tabs) as string
                int new_indent = indent+1;
                sprintf(new_indent_val, "%d", new_indent);
                execl("proctree", "proctree", new_node, new_indent_val, NULL); //execution in the child process (with the child node's arguments)
                printf("ERROR: exec() function not working properly!\n");
                exit(0);
            }
            int status;
            waitpid(pid, &status, 0); //wait for the child process (created above)
        }

        break;
    }

    if(found==0){//if the given node is not found
        printf("City %s not found\n", argv[1]);
    }
    fclose(fptr); //closing the opened text file

    return 0;
}