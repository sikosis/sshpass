#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
    struct termios old_settings, settings;
    char password[128];
    const char *prompt = argc > 1 ? argv[1] : "Password:";

    if (!isatty(STDIN_FILENO))
        return 10;
    if (tcgetattr(STDIN_FILENO, &old_settings) != 0)
        return 11;
    settings = old_settings;
    settings.c_lflag &= (tcflag_t)~ECHO;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) != 0)
        return 12;
    fputs(prompt, stderr);
    fflush(stderr);
    if (fgets(password, sizeof(password), stdin) == NULL)
        return 13;
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    password[strcspn(password, "\r\n")] = '\0';
    if (strcmp(password, "swordfish") != 0) {
        fputs("\nPassword:", stderr);
        fflush(stderr);
        (void)fgets(password, sizeof(password), stdin);
        return 9;
    }
    puts("\nauthenticated");
    return 0;
}
