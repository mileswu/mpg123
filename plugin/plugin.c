
#include <stdlib.h>

#include "plugin.h"

static int plugin_init(struct config *c);
static int plugin_exit(struct config *c);
static int plugin_action(struct config *c);

struct config c = {
  NULL, 
  plugin_init,
  plugin_exit,
  plugin_action,
  0,
  "Plugin Demo!",
  NULL
};


struct config *plugin_get_config(void)
{
	return &c;
}


int plugin_init(struct config *c)
{

}

int plugin_exit(struct config *c)
{

}

int plugin_action(struct config *c)
{

}
