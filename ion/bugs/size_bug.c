#include <assert.h>
#include <string.h>

int fetch_value(void* dest_ptr, size_t* dest_size_ptr)
{
  assert(dest_ptr && dest_size_ptr);
  int const value = 42;
  if (*dest_size_ptr == sizeof(value))
  {
    memcpy(dest_ptr, &value, sizeof(value));
    return 0;
  }
  return -1;
}
