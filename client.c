#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clientcommands.h"
#include "networking.h"
#include "sharedfunctions.h"

int main(int argc, char** argv) {

    /* 
     * check the arguments passed in for the type of command and its arguments
     * then check if the argument count matches the given command, and create a command struct
     * then call the associated function which will execute the necessary code for the function.
     */

    ClientCommand* command = malloc(sizeof(ClientCommand)); 
    int (*cmnd_func)(ClientCommand*);

    if (argc < 3 || argc > 4) {		// all commands have at least 3 arguments or at most 4
        command->type = 0;
        command->args = malloc(sizeof(char*));		// creating an invalid command rather then just exiting so that memory freeing can be all handled at the end
        command->args[0] = "Invalid argument count";
        cmnd_func = &_invalidcommand;
        goto skip;			// skip all other argument parsing
    }

    // figure out what type of command is being executed
    char* command_text = argv[1];
    int arg_count = 1;			// most commands have 1 argument, so that is the default
    if (strcmp(command_text, "configure") == 0) {
        arg_count = 2;			// set the expected number of arguments that should be passed to the program
        command->type = configure;	// set the type of command that was given
        cmnd_func = &_configure;	// assign the function to be called if command was structured correctly
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
        arg_count = 2;
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

    if (argc - 2 != arg_count) {	// check that given argument count matches expected argument count
        command->type = 0;
        command->args = malloc(sizeof(char*));
        command->args[0] = "Invalid argument count";
        cmnd_func = &_invalidcommand;    
        goto skip;
    }

    // retrieve arguments, save them in the command struct
    // command->args = command->type == checkout ? malloc(sizeof(char*) * 2) : malloc(sizeof(char*) * arg_count);
    command->args = malloc(sizeof(char*) * arg_count);
    command->args[0] = argv[2];
    // if (command->type == checkout) command->args[1] = ".";
    if (arg_count == 2) command->args[1] = argv[3];

    //execute associated function
    skip: ;
    int ret = (*cmnd_func)(command);	// execute command's associated function
    free(command->args);
    free(command);

    return ret;
}
