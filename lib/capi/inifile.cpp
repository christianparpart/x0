#include <x0/capi/inifile.h>
#include <x0/IniFile.h>

using namespace x0;

struct s_x0_inifile {
    IniFile obj;
};

x0_inifile_t* x0_inifile_create()
{
    return new x0_inifile_t();
}

void x0_inifile_destroy(x0_inifile_t* ini)
{
    delete ini;
}

int x0_inifile_load(x0_inifile_t* ini, const char* path)
{
    return ini->obj.loadFile(path);
}

int x0_inifile_save(x0_inifile_t* ini, const char* path)
{
    return -1; // TODO
}

void x0_inifile_clear(x0_inifile_t* ini)
{
    ini->obj.clear();
}

size_t x0_inifile_section_count(x0_inifile_t* ini)
{
    return ini->obj.size();
}

size_t x0_inifile_section_size(x0_inifile_t* ini, const char* title)
{
    return ini->obj.get(title).size();
}

int x0_inifile_section_contains(x0_inifile_t* ini, const char* title)
{
    return ini->obj.contains(title);
}

void x0_inifile_section_remove(x0_inifile_t* ini, const char* title)
{
    ini->obj.remove(title);
}

void x0_inifile_value_set(x0_inifile_t* ini, const char* title, const char* key, const char* value)
{
    ini->obj.set(title, key, value);
}

ssize_t x0_inifile_value_get(x0_inifile_t* ini, const char* title, const char* key, char* value, size_t size)
{
    if (size == 0)
        return 0;

    std::string result;
    if (!ini->obj.get(title, key, result))
        return 0;

    size_t n = std::min(size - 1, result.size());
    memcpy(value, result.data(), n);
    value[n] = '\0';
    return n;
}

