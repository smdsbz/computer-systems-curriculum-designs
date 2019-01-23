#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <gtk/gtk.h>


/******* Payloads *******/

#define PAYLOAD_PROCESS_NUM     ( 3 )

typedef char bool;
typedef enum _payload_control_char_t { NONE, STOP_ON_DONE } payload_control_char_t;

typedef struct _PayloadControlBlock {
    int                     id;
    pid_t                   process_id;
    bool                    is_running;
    long                    start_timestamp;
    double                  scale_factor;       // in microseconds (us), should be no more than 1s, i.e. 1e7us
    double                  result;
    payload_control_char_t  ctl;
} PayloadControlBlock;

PayloadControlBlock *payload_controls = NULL;

double some_workload(PayloadControlBlock *pcb) {
    double interval = pcb->scale_factor * rand() / RAND_MAX;
    pcb->result = interval;
    usleep((unsigned)interval);
    return interval;
}

void payload_process_fn(PayloadControlBlock *pcb) {
    srand(pcb->process_id);
    while (pcb->ctl != STOP_ON_DONE) {
        pcb->is_running = TRUE;
        // do some virtual calculations
        pcb->start_timestamp = g_get_monotonic_time();
        some_workload(pcb);
        pcb->is_running = FALSE;
    }
    return;
}


/******* Helper Functions *******/


/******* Main *******/

guint render_timer;

int sync_adjustment_to_pcb(GtkAdjustment *adjust, void *id) {
    payload_controls[(long)id].scale_factor = gtk_adjustment_get_value(adjust);
    return TRUE;
}

int update_gui(void *toplevel) {
    GtkBuilder *builder = toplevel;
    /* gtk_progress_bar_pulse(GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "win0_progress-bar"))); */
    // set text displays
    for (unsigned id = 0; id != PAYLOAD_PROCESS_NUM; ++id) {
        static char fmtstr[1024];
        static char widget_id[256];
        // set progress bar
        sprintf(widget_id, "win%u_progress-bar", id);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gtk_builder_get_object(builder, widget_id)), (g_get_monotonic_time() - payload_controls[id].start_timestamp) / payload_controls[id].result);
        // set text output
        sprintf(fmtstr, "ETA for this round is %.2f ms.\n", payload_controls[id].result / 1e3);
        sprintf(widget_id, "win%u_text-buffer", id);
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(gtk_builder_get_object(builder, widget_id)), fmtstr, -1);
    }
    return TRUE;
}

void destroy_job(void) {
    g_source_remove(render_timer);
    // wait for all payload processes to quit
    for (unsigned id = 0; id != PAYLOAD_PROCESS_NUM; ++id) {
        payload_controls[id].ctl = STOP_ON_DONE;
        if (payload_controls[id].process_id != 0) {
            g_print("destroy_job(): waiting for %u to quit...\n", id);
            waitpid(payload_controls[id].process_id, NULL, 0);
        }
    }
    // end GTK event loop
    gtk_main_quit();
    return;
}

int main(int argc, char **argv) {

    // initialize payload runtime (as shared memory)
    int payload_controls_shmid;
    if ((payload_controls_shmid = shmget(IPC_PRIVATE, PAYLOAD_PROCESS_NUM * sizeof(PayloadControlBlock), IPC_CREAT | 0600)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    if ((payload_controls = (PayloadControlBlock *)shmat(payload_controls_shmid, NULL, 0)) == (void *)-1) {
        perror("shmat");
    }
    for (unsigned id = 0; id != PAYLOAD_PROCESS_NUM; ++id) {
        // initialize pcbs
        payload_controls[id].id = id;
        payload_controls[id].process_id = 0;
        payload_controls[id].is_running = FALSE;
        payload_controls[id].start_timestamp = 0;
        payload_controls[id].scale_factor = 2e6;
        payload_controls[id].result = 0.0;
        payload_controls[id].ctl = NONE;
    }
    for (unsigned id = 0; id != PAYLOAD_PROCESS_NUM; ++id) {
        pid_t pid;
        if ((pid = fork()) == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            payload_process_fn(&payload_controls[id]);
            exit(EXIT_SUCCESS);
        }
        payload_controls[id].process_id = pid;
    }

    // late-build GUI
    GtkBuilder *builder;
    GObject *toplevel;
    GObject *pbar;

    gtk_init(&argc, &argv);

    if ((builder = gtk_builder_new_from_file("./window.glade")) == NULL) {
        g_printerr("gtk_builder_new_from_file\n");
        return EXIT_FAILURE;
    }

    // get toplevel handler, and register destroy signals
    toplevel = gtk_builder_get_object(builder, "toplevel");
    g_signal_connect(toplevel, "destroy", destroy_job, NULL);

    /* // find progress bar, and animate it */
    /* pbar = gtk_builder_get_object(builder, "win0_progress-bar"); */
    /* gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(pbar), 3e-2); */
    render_timer = g_timeout_add(20, update_gui, (void *)builder);

    // sync pcb settings to GUI and register value-changed callbacks
    for (unsigned id = 0; id != PAYLOAD_PROCESS_NUM; ++id) {
        static char widget_id[256];
        sprintf(widget_id, "win%u_adjustment", id);
        static GObject *adjust;
        adjust = gtk_builder_get_object(builder, widget_id);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(adjust), payload_controls[id].scale_factor);
        g_signal_connect(adjust, "value-changed", G_CALLBACK(sync_adjustment_to_pcb), (void *)(long)id);
    }

    // start and enter event loop
    gtk_widget_show_all(GTK_WIDGET(toplevel));
    gtk_main();

    // release memory resources
    if (shmdt(payload_controls) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    payload_controls = NULL;

    return EXIT_SUCCESS;
}

