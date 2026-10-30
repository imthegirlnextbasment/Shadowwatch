#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_RECORDS 200
#define MAX_NAME 50
#define MAX_LINE 200
#define ALERT_THRESHOLD 3

/* Tactical terminal palette: red = alert, green = verified, cyan = chrome. */
#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_DIM    "\033[2m"
#define ANSI_RED    "\033[1;31m"
#define ANSI_GREEN  "\033[1;32m"
#define ANSI_CYAN   "\033[1;36m"
#define ANSI_YELLOW "\033[1;33m"
#define ANSI_WHITE  "\033[1;37m"

void terminal_init(void) {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

void clear_screen(void) {
    printf("\033[2J\033[H");
}

typedef struct {
    char username[MAX_NAME];
    int failed;
    int successful;
} Account;

int count_flagged(Account accounts[], int count) {
    int i, flagged = 0;
    for (i = 0; i < count; i++) {
        if (accounts[i].failed >= ALERT_THRESHOLD) flagged++;
    }
    return flagged;
}

void print_status_strip(int analyzed, int account_count, int total_lines,
                        int flagged_count) {
    const char *state = analyzed ? ANSI_GREEN "ONLINE" : ANSI_YELLOW "STANDBY";
    printf("\n" ANSI_DIM "+-- NODE STATUS ------------------------------------------------+\n" ANSI_RESET);
    printf("| STATE: %s" ANSI_RESET "  | ACCOUNTS: %-3d | EVENTS: %-3d | ALERTS: %-3d |\n",
           state, account_count, total_lines, flagged_count);
    printf(ANSI_DIM "+---------------------------------------------------------------+\n" ANSI_RESET);
}

void print_meter(const char *label, int value, int maximum) {
    int i, filled = 0;
    if (maximum > 0) filled = (value * 12) / maximum;
    printf("  %-12s [", label);
    for (i = 0; i < 12; i++) {
        if (i < filled) printf(ANSI_GREEN "#" ANSI_RESET);
        else printf(".");
    }
    printf("] %d\n", value);
}

void show_event_profile(Account accounts[], int count) {
    int i, failed = 0, successful = 0, total;
    for (i = 0; i < count; i++) {
        failed += accounts[i].failed;
        successful += accounts[i].successful;
    }
    total = failed + successful;
    printf("\n" ANSI_CYAN "EVENT PROFILE\n" ANSI_RESET);
    print_meter("FAILED", failed, total);
    print_meter("SUCCESS", successful, total);
}

void print_banner(void) {
    printf("\n" ANSI_CYAN);
    printf("##### #   #  ###  ####   ###  #   # #   #  ###  #####  #### #   #\n");
    printf("#     #   # #   # #   # #   # #   # #   # #   #   #   #     #   #\n");
    printf("##### ##### ##### #   # #   # # # # # # # #####   #   #     #####\n");
    printf("    # #   # #   # #   # #   # ## ## ## ## #   #   #   #     #   #\n");
    printf("##### #   # #   # ####   ###  #   # #   # #   #   #    #### #   #\n");
    printf("\n                 Made by Ameena Mahin\n" ANSI_RESET);
}

void pause_screen(void) {
    printf("\n" ANSI_DIM "[ ENTER ] continue   [ CTRL+C ] abort" ANSI_RESET);
    getchar();
}

int find_account(Account accounts[], int count, const char *username) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(accounts[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

/* Log format: username,FAILED or username,SUCCESS */
int analyze_logs(Account accounts[], int *account_count, int *total_lines) {
    FILE *file = fopen("login_logs.txt", "r");
    char line[MAX_LINE];
    int valid_records = 0;
    *account_count = 0;
    *total_lines = 0;

    if (file == NULL) {
        printf("\n[!] Could not open login_logs.txt\n");
        printf("    Put login_logs.txt in the same folder as the program.\n");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char username[MAX_NAME];
        char status[20];
        char *comma;
        int index;
        size_t len;

        (*total_lines)++;
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        comma = strchr(line, ',');
        if (comma == NULL) {
            printf("[!] Skipping malformed line %d\n", *total_lines);
            continue;
        }

        *comma = '\0';
        if (strlen(line) == 0 || strlen(comma + 1) == 0 ||
            strlen(line) >= MAX_NAME || strlen(comma + 1) >= sizeof(status)) {
            printf("[!] Skipping malformed line %d\n", *total_lines);
            continue;
        }

        strcpy(username, line);
        strcpy(status, comma + 1);

        if (strcmp(status, "FAILED") != 0 && strcmp(status, "SUCCESS") != 0) {
            printf("[!] Skipping unknown status on line %d\n", *total_lines);
            continue;
        }

        index = find_account(accounts, *account_count, username);
        if (index == -1) {
            if (*account_count >= MAX_RECORDS) {
                printf("[!] Account limit reached; remaining records ignored.\n");
                break;
            }
            index = *account_count;
            strcpy(accounts[index].username, username);
            accounts[index].failed = 0;
            accounts[index].successful = 0;
            (*account_count)++;
        }

        if (strcmp(status, "FAILED") == 0) {
            accounts[index].failed++;
        } else {
            accounts[index].successful++;
        }
        valid_records++;
    }

    fclose(file);
    return valid_records;
}

void show_flagged(Account accounts[], int count) {
    int i;
    int found = 0;

    printf("\n" ANSI_RED "+-- FLAGGED ACCOUNTS ---------------------------+\n" ANSI_RESET);
    printf(ANSI_YELLOW "| ALERT RULE: %d OR MORE FAILED LOGINS           |\n" ANSI_RESET, ALERT_THRESHOLD);
    printf(ANSI_RED "+--------------------------------------------------+\n" ANSI_RESET);

    for (i = 0; i < count; i++) {
        if (accounts[i].failed >= ALERT_THRESHOLD) {
            printf(ANSI_RED "[!! ALERT !!]" ANSI_RESET " %-16s " ANSI_YELLOW "FAILED: %d" ANSI_RESET " | " ANSI_GREEN "SUCCESS: %d" ANSI_RESET "\n",
                   accounts[i].username, accounts[i].failed,
                   accounts[i].successful);
            found = 1;
        }
    }

    if (!found) {
        printf(ANSI_GREEN "[ OK ] No accounts crossed the alert threshold.\n" ANSI_RESET);
    }
}

void show_summary(Account accounts[], int count, int total_lines, int valid_records) {
    int i;
    int failed_total = 0;
    int success_total = 0;
    int flagged_total = 0;

    for (i = 0; i < count; i++) {
        failed_total += accounts[i].failed;
        success_total += accounts[i].successful;
        if (accounts[i].failed >= ALERT_THRESHOLD) {
            flagged_total++;
        }
    }

    printf("\n" ANSI_CYAN "+-- SECURITY SUMMARY ---------------------------+\n" ANSI_RESET);
    printf("| LOG LINES READ       : %-25d |\n", total_lines);
    printf("| VALID LOGIN RECORDS  : %-25d |\n", valid_records);
    printf("| UNIQUE ACCOUNTS      : %-25d |\n", count);
    printf("| FAILED LOGIN ATTEMPTS: %-25d |\n", failed_total);
    printf("| SUCCESSFUL LOGINS    : %-25d |\n", success_total);
    printf("| FLAGGED ACCOUNTS     : %-25d |\n", flagged_total);
    printf("| ALERT THRESHOLD      : %-15d FAILED  |\n", ALERT_THRESHOLD);
    printf(ANSI_CYAN "+--------------------------------------------------+\n" ANSI_RESET);
}

void write_report(Account accounts[], int count, int total_lines, int valid_records) {
    FILE *report = fopen("incident_report.txt", "w");
    int i;
    int failed_total = 0;
    int success_total = 0;
    int flagged_total = 0;

    if (report == NULL) {
        printf("\n[!] Could not create incident_report.txt\n");
        return;
    }

    for (i = 0; i < count; i++) {
        failed_total += accounts[i].failed;
        success_total += accounts[i].successful;
        if (accounts[i].failed >= ALERT_THRESHOLD) {
            flagged_total++;
        }
    }

    fprintf(report, "============================================\n");
    fprintf(report, "       SHADOWWATCH INCIDENT REPORT\n");
    fprintf(report, "============================================\n");
    fprintf(report, "Note: This report is based on sample log data.\n\n");
    fprintf(report, "Log lines read       : %d\n", total_lines);
    fprintf(report, "Valid login records  : %d\n", valid_records);
    fprintf(report, "Unique accounts      : %d\n", count);
    fprintf(report, "Failed login attempts: %d\n", failed_total);
    fprintf(report, "Successful logins    : %d\n", success_total);
    fprintf(report, "Flagged accounts     : %d\n\n", flagged_total);
    fprintf(report, "FLAGGED ACCOUNTS\n");
    fprintf(report, "----------------\n");

    for (i = 0; i < count; i++) {
        if (accounts[i].failed >= ALERT_THRESHOLD) {
            fprintf(report, "[ALERT] %s | Failed: %d | Successful: %d\n",
                    accounts[i].username, accounts[i].failed,
                    accounts[i].successful);
        }
    }

    if (flagged_total == 0) {
        fprintf(report, "No accounts crossed the alert threshold.\n");
    }

    fclose(report);
    printf("\n[+] Report saved as incident_report.txt\n");
}

int main(void) {
    Account accounts[MAX_RECORDS];
    int account_count = 0;
    int total_lines = 0;
    int valid_records = 0;
    int analyzed = 0;
    int choice;
    int ch;

    terminal_init();

    do {
        clear_screen();
        print_banner();
        print_status_strip(analyzed, account_count, total_lines,
                           analyzed ? count_flagged(accounts, account_count) : 0);
        printf("\n" ANSI_WHITE "  +-- OPERATIONS --------------------------------+\n" ANSI_RESET);
        printf("  " ANSI_CYAN "[1]" ANSI_RESET " Analyse login logs\n");
        printf("  " ANSI_CYAN "[2]" ANSI_RESET " View flagged accounts\n");
        printf("  " ANSI_CYAN "[3]" ANSI_RESET " View security summary\n");
        printf("  " ANSI_CYAN "[4]" ANSI_RESET " Generate incident report\n");
        printf("  " ANSI_RED   "[5]" ANSI_RESET " Exit\n");
        printf("  " ANSI_WHITE "+-----------------------------------------------+\n" ANSI_RESET);
        printf("\n  " ANSI_YELLOW ">>> SELECT OPERATION: " ANSI_RESET);

        if (scanf("%d", &choice) != 1) {
            printf("\n[!] Please enter a number from 1 to 5.\n");
            while ((ch = getchar()) != '\n' && ch != EOF) {
                /* Clear invalid input */
            }
            pause_screen();
            continue;
        }
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* Clear any extra input on the line */
        }

        switch (choice) {
            case 1:
                valid_records = analyze_logs(accounts, &account_count, &total_lines);
                if (valid_records > 0) {
                    analyzed = 1;
                    printf("\n" ANSI_GREEN "[ OK ] ANALYSIS COMPLETE" ANSI_RESET
                           " // Processed %d valid records.\n",
                           valid_records);
                } else {
                    analyzed = 0;
                    printf("\n[!] No valid records were analysed.\n");
                }
                pause_screen();
                break;

            case 2:
                if (analyzed) {
                    show_flagged(accounts, account_count);
                } else {
                    printf("\n[!] Please analyse the logs first (option 1).\n");
                }
                pause_screen();
                break;

            case 3:
                if (analyzed) {
                    show_summary(accounts, account_count, total_lines, valid_records);
                    show_event_profile(accounts, account_count);
                } else {
                    printf("\n[!] Please analyse the logs first (option 1).\n");
                }
                pause_screen();
                break;

            case 4:
                if (analyzed) {
                    write_report(accounts, account_count, total_lines, valid_records);
                } else {
                    printf("\n[!] Please analyse the logs first (option 1).\n");
                }
                pause_screen();
                break;

            case 5:
                printf("\n[+] ShadowWatch shutting down. Stay secure!\n");
                break;

            default:
                printf("\n[!] Invalid choice. Please select 1 to 5.\n");
                pause_screen();
        }
    } while (choice != 5);

    return 0;
}
