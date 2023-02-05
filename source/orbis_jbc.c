#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

// Variables for (un)jailbreaking
jbc_cred g_Cred;
jbc_cred g_RootCreds;

// Verify jailbreak
static int is_jailbroken()
{
    FILE *s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
        return 0;

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return 1;
}

// Jailbreaks creds
static int jailbreak()
{
    if (is_jailbroken())
    {
        return 1;
    }

    jbc_get_cred(&g_Cred);
    g_RootCreds = g_Cred;
    jbc_jailbreak_cred(&g_RootCreds);
    jbc_set_cred(&g_RootCreds);

    return (is_jailbroken());
}

// Initialize jailbreak
int initialize_jbc()
{
    // Pop notification depending on jailbreak result
    if (!jailbreak())
    {
        return 0;
    }

    return 1;
}

// Unload libjbc libraries
void terminate_jbc()
{
    if (!is_jailbroken())
        return;

    // Restores original creds
    jbc_set_cred(&g_Cred);
}
