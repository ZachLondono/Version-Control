#include <stdlib.h>
#include <stdio.h>

int main() {

    // setup
    system("make");
    system("mkdir server && mkdir client1 && mkdir client2");
    system("cp WTFserver server/ && cp WTF client1/ && cp WTF client2/");

    system("(cd server && ./WTFserver 5556&)");

    printf("********Error Case***********\n");
    system("(cd client1 && ./WTF configure localhost 5555)");
    system("(cd client1 && ./WTF create proj)");
    printf("*****************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client1 && ./WTF configure localhost 5556)");
    system("(cd client1 && ./WTF create proj)");
    printf("******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client1 && echo \"hello\" >> proj/file)");
    system("(cd client1 && ./WTF add proj file)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client1 && echo \"hello\" >> proj/file2)");
    system("(cd client1 && ./WTF add proj file2)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client1 && echo \"hello\" >> proj/file2)");
    system("(cd client1 && ./WTF add proj file2)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client2 && ./WTF configure localhost 5556)");
    system("(cd client2 && ./WTF checkout proj)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client1 && ./WTF commit proj)");
    system("(cd client1 && ./WTF push proj)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client2 && echo \"hello\" >> proj/file2)");
    system("(cd client2 && ./WTF add proj file2)");
    system("(cd client2 && echo \"hello\" >> proj/file3)");
    system("(cd client2 && ./WTF add proj file3)");
    system("(cd client2 && ./WTF remove proj file2)");
    printf("*******************************\n\n");

    printf("********Failure Case***********\n");
    system("(cd client2 && ./WTF commit proj)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client2 && ./WTF update proj)");
    system("(cd client2 && ./WTF upgrade proj)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client2 && ./WTF curretversion proj)");
    printf("*******************************\n\n");

    printf("********Success Case***********\n");
    system("(cd client2 && ./WTF history proj)");
    printf("*******************************\n\n");


    printf("**********\n");
    printf("Server must be manually shut down\nuse 'ps aux | grep WTFserver' to get pid\n");
    printf("**********\n");
    
}
