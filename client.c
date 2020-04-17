#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clientcommands.h"
#include "networking.h"
#include "sharedfunctions.h"

int main(int argc, char** argv) {

    ClientCommand* command = malloc(sizeof(ClientCommand)); 
    int (*cmnd_func)(ClientCommand*);

    if (argc < 3 || argc > 4) {
        command->type = 0;
        command->args = malloc(sizeof(char*));
        command->args[0] = "Invalid argument count";
        cmnd_func = &_invalidcommand;
        goto skip;
    }

    // figure out what type of command is being executed
    char* command_text = argv[1];
    int arg_count = 1;
    if (strcmp(command_text, "configure") == 0) {
        arg_count = 2;
        command->type = configure;
        cmnd_func = &_configure;
    } else if (strcmp(command_text, "checkout") == 0) {             
        command->type = checkout;
        cmnd_func = &_checkout;
    } else if (strcmp(command_text, "commit") == 0) {                
        command->type = commit;
        cmnd_func = &_commit;
    } else if (strcmp(command_text, "create") == 0) {
        command->type = create;
        cmnd_func = &_create;
    } else if (strcmp(command_text, "currentversion") == 0) {
        command->type = currentversion;
        cmnd_func = &_currentversion;
    } else if (strcmp(command_text, "add") == 0) {
        arg_count = 2;
        command->type = add;
        cmnd_func = &_add;
    } else if (strcmp(command_text, "remove") == 0) {
        arg_count = 2;
        command->type = remove_cmnd;
        cmnd_func = &_remove;
    } else if (strcmp(command_text, "rollback") == 0) {   
        command->type = rollback;
        cmnd_func = &_rollback;
    } else if (strcmp(command_text, "update") == 0) {
            command->type = update;
            cmnd_func = &_update;
    } else if (strcmp(command_text, "push") == 0) {
            command->type = push;
            cmnd_func = &_push;
    } else if (strcmp(command_text, "destroy") == 0) {
            command->type = destroy;
            cmnd_func = &_destroy;
    } else if (strcmp(command_text, "history") == 0) {
            command->type = history;
            cmnd_func = &_history;
    } else {
        command->type = 0;
        command->args = malloc(sizeof(char*));
        command->args[0] = "Invalid command";
        cmnd_func = &_invalidcommand;
        goto skip;
    }

    if (argc - 2 != arg_count) {
        command->type = 0;
        command->args = malloc(sizeof(char*));
        command->args[0] = "Invalid argument count";
        cmnd_func = &_invalidcommand;    
        goto skip;
    }

    // retrieve arguments
    command->args = malloc(sizeof(char*) * arg_count);
    command->args[0] = argv[2];
    if (arg_count == 2) command->args[1] = argv[3];

    //execute associated function
    skip: ;
    int ret = (*cmnd_func)(command);
    free(command->args);
    free(command);

    return ret;
}
