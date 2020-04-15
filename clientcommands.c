#include "clientcommands.h"

int executeCommand(int argc, char** argv) {

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

//invalid=0, configure=1, checkout=2, update=3, upgrade=4, commit=5, push=6, create=7, destroy=8, add=9, remove_cmnd=10, currentversion=11, history=12, rollback=13

int _invalidcommand(ClientCommand* command) {
    //TODO maybe set type of invalid command in parse command?
    printf("Fatal Error: %s\n", command->args[0]);
    return 0;
}

int _configure(ClientCommand* command) {
    printf("config not implimented\n");
    return -1;
}

int _checkout(ClientCommand* command) {
    printf("checkout not implimented\n");
    return -1;
}

int _update(ClientCommand* command) {
    printf("update not implimented\n");
    return -1;
}

int _upgrade(ClientCommand* command) {
    printf("upgrade not implimented\n");
    return -1;
}

int _commit(ClientCommand* command) {
    printf("commit not implimented\n");
    return -1;
}

int _push(ClientCommand* command) {
    printf("push not implimented\n");
    return -1;
}

int _create(ClientCommand* command) {
    printf("create not implimented\n");
    return -1;
}

int _destroy(ClientCommand* command) {
    printf("destroy not implimented\n");
    return -1;
}

int _add(ClientCommand* command) {
    printf("add not implimented\n");
    return -1;
}

int _remove(ClientCommand* command) {
    printf("remove not implimented\n");
    return -1;
}

int _currentversion(ClientCommand* command) {
    printf("currentversion not implimented\n");
    return -1;
}

int _history(ClientCommand* command) {
    printf("history not implimented\n");
    return -1;
}

int _rollback(ClientCommand* command) {
    printf("rollback not implimented\n");
    return -1;
}