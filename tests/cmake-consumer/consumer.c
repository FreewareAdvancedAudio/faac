/* Calls one exported function per linked library, so a package that finds the
 * wrong file or misses a dependency such as libm fails here at link or run time. */
#include <stdio.h>

#ifdef USE_FAAC
#include <faac.h>
#endif
#ifdef USE_FAAD3
#include <faad3.h>
#endif
#ifdef USE_FAAM
#include <faam.h>
#endif
#ifdef USE_FAAR
#include <faar.h>
#endif

int main(void)
{
    int failures = 0;
#ifdef USE_FAAC
    faac_library_info faac_info = { sizeof(faac_library_info) };
    failures += faac_get_library_info(&faac_info) != 0;
#endif
#ifdef USE_FAAD3
    faad_library_info faad_info = { sizeof(faad_library_info) };
    failures += faad_get_library_info(&faad_info) != 0;
#endif
#ifdef USE_FAAM
    faam_library_info faam_info = { sizeof(faam_library_info) };
    failures += faam_get_library_info(&faam_info) != 0;
#endif
#ifdef USE_FAAR
    faar_library_info faar_info = { sizeof(faar_library_info) };
    failures += faar_get_library_info(&faar_info) != 0;
#endif
    if (failures) {
        fprintf(stderr, "%d library info calls failed\n", failures);
        return 1;
    }
    return 0;
}
