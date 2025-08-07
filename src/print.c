# include <../include/all.h>

void print_results(const t_task *tasks, const uint16_t num_tasks) {
    printf("[*] Scan results:\n");
    for (uint16_t i = 0; i < num_tasks; i++) {
        const char *result_str;
        switch (tasks[i].result) {
            case OPEN:
                result_str = "OPEN      ";
                break;
            case CLOSED:
                result_str = "CLOSED    ";
                break;
            case FILTERED:
                result_str = "FILTERED  ";
                break;
            default:
                result_str = "ERROR     ";
        }
        printf("%s | %s %d\n", result_str, inet_ntoa(tasks[i].probe.addr.sin_addr), ntohs(tasks[i].probe.addr.sin_port));
    }
}
