#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main() {
    while (1) {
        time_t now = time(NULL);
        printf("%s", ctime(&now));
        sleep(5); // Sleep for 5 seconds
    }
    return 0;
}
