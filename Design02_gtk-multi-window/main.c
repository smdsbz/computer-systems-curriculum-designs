#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>


/******* Helper *******/

int update_pbar(void *pbar) {
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pbar));
    return TRUE;
}

guint ptimer;

void destroy_job(void) {
    g_print("in destroy_job()\n");
    g_source_remove(ptimer);
    gtk_main_quit();
}


/******* Main *******/

int main(int argc, char **argv) {

    GtkBuilder *builder;
    GObject *toplevel;
    GObject *pbar;

    gtk_init(&argc, &argv);

    if ((builder = gtk_builder_new_from_file("window.glade")) == NULL) {
        g_printerr("gtk_builder_new_from_file\n");
        return EXIT_FAILURE;
    }

    // get toplevel handler, and register destroy signals
    toplevel = gtk_builder_get_object(builder, "toplevel");
    g_signal_connect(toplevel, "destroy", destroy_job, NULL);

    // find progress bar, and animate it
    pbar = gtk_builder_get_object(builder, "win0_progress-bar");
    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(pbar), 1e-2);
    ptimer = g_timeout_add(10, update_pbar, (void *)pbar);

    // start and enter event loop
    gtk_widget_show_all(GTK_WIDGET(toplevel));
    gtk_main();

    return EXIT_SUCCESS;
}

