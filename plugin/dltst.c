
#include <dlfcn.h>
#include <stdio.h>

#include "plugin.h"

int main(int argc, char **argv) {
     void *h;
     struct config *(*plugin)(void);
     struct config *c;

     h = dlopen ("./plugin.so", RTLD_LAZY);
     if(!h) {
       fprintf(stderr,"Can't open dynamic plugin!\n");
       exit(1);
     }

     plugin = dlsym(h, "plugin_get_config");
     if(!plugin)  {
        fprintf(stderr,"Can't get plugin_get_config symbol from dynamic plugin!\n");
	exit(1);
     }
     
     c = (*plugin)();
     printf ("%s\n", c->name);
     dlclose(h);
}

