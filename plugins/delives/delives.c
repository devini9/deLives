#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("delives", "en-US")

extern void create_delives_dock(void);
extern void destroy_delives_dock(void);

bool obs_module_load(void)
{
	create_delives_dock();
	return true;
}

void obs_module_unload(void)
{
	destroy_delives_dock();
}
