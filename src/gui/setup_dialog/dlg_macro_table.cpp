#include "dlg_macro_table.h"

#include <string.h>

#include <gtk/gtk.h>
#include <libintl.h>

#include "keycons.h"
#include "mactab.h"

#define _(str) gettext(str)


#define STR_NULL_ITEM "..."
#define MACRO_DEFAULT_VALUE _("(replace text)")

enum {COL_KEY = 0, COL_VALUE, NUM_COLS};


void check_last_macro_in_list(GtkListStore* list);
void key_edited_cb (GtkCellRendererText *celltext,
                    const gchar *string_path,
                    const gchar *newkey,
                    gpointer user_data);
void value_edited_cb(GtkCellRendererText *celltext,
                     const gchar *string_path,
                     const gchar *newvalue,
                     gpointer data);
void remove_macro_clicked_cb(GtkButton *button, gpointer user_data);
void removeall_macro_clicked_cb(GtkButton *button, gpointer user_data);
void import_macro_clicked_cb(GtkButton *button, gpointer user_data);
void export_macro_clicked_cb(GtkButton *button, gpointer user_data);

GtkWidget* unikey_macro_dialog_new()
{
    GtkBuilder* builder = gtk_builder_new();

    gtk_builder_add_from_file(builder, UI_DATA_DIR "/setup-macro.ui", NULL);

    GtkWidget* dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dlg_macro_table"));

    // init macro list
    GtkListStore* list = GTK_LIST_STORE(gtk_builder_get_object(builder, "list_macro"));
    check_last_macro_in_list(list);

    GtkTreeView* tree = GTK_TREE_VIEW(gtk_builder_get_object(builder, "tree_macro"));
    g_object_set_data(G_OBJECT(dialog), "tree_macro", tree);

    // connect signal
    g_signal_connect(gtk_builder_get_object(builder, "cellrenderertext_key"),
                     "edited", G_CALLBACK(key_edited_cb), tree);

    g_signal_connect(gtk_builder_get_object(builder, "cellrenderertext_value"),
                     "edited", G_CALLBACK(value_edited_cb), tree);

    g_signal_connect(gtk_builder_get_object(builder, "btn_remove"),
                     "clicked", G_CALLBACK(remove_macro_clicked_cb), tree);

    g_signal_connect(gtk_builder_get_object(builder, "btn_removeall"),
                     "clicked", G_CALLBACK(removeall_macro_clicked_cb), tree);

    g_signal_connect(gtk_builder_get_object(builder, "btn_import"),
                     "clicked", G_CALLBACK(import_macro_clicked_cb), dialog);

    g_signal_connect(gtk_builder_get_object(builder, "btn_export"),
                     "clicked", G_CALLBACK(export_macro_clicked_cb), dialog);


    g_object_unref(builder);

    return dialog;
}

void append_macro_to_list_store(GtkListStore* list, CMacroTable macro)
{
    GtkTreeIter iter;
    gchar key[MAX_MACRO_KEY_LEN*3];
    gchar value[MAX_MACRO_TEXT_LEN*3];
    gchar* oldkey;
    UKBYTE* p;
    int inLen, maxOutLen;
    int i, ret;

    for (i = 0 ; i < macro.getCount(); i++)
    {
        // get key and convert to XUTF charset
        p = (UKBYTE*)macro.getKey(i);
        inLen = -1;
        maxOutLen = sizeof(key);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE*)key,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        // check if any key same as newkey
        gboolean b = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter);
        while (b)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, COL_KEY, &oldkey, -1);

            if (strcasecmp(oldkey, key) == 0)
            {
                break;
            }

            b = gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &iter);
        }
        if (b)
        {
            continue;
        }
        // end check

        // get value and convert to XUTF charset
        p = (UKBYTE*)macro.getText(i);
        inLen = -1;
        maxOutLen = sizeof(value);
        ret = VnConvert(CONV_CHARSET_VNSTANDARD, CONV_CHARSET_XUTF8,
                        p, (UKBYTE*)value,
                        &inLen, &maxOutLen);
        if (ret != 0)
            continue;

        // append to liststore
        gtk_list_store_append(list, &iter);
        gtk_list_store_set(list, &iter, COL_KEY, key, COL_VALUE, value, -1);
    }
}

void unikey_macro_dialog_load_macro(GtkDialog* dialog, CMacroTable macro)
{
    GtkTreeView* tree;
    GtkListStore* list;
    GtkTreeIter iter;

    tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(dialog), "tree_macro"));
    list = GTK_LIST_STORE(gtk_tree_view_get_model(tree));
    
    gtk_list_store_clear(list);

    append_macro_to_list_store(list, macro);
    check_last_macro_in_list(list);

    // select first iter
    GtkTreeSelection* select = gtk_tree_view_get_selection(tree);
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter);
    gtk_tree_selection_select_iter(select, &iter);
}

void unikey_macro_dialog_save_macro(GtkDialog* dialog, CMacroTable* macro)
{
    GtkTreeView* tree;
    GtkTreeModel* model;
    GtkTreeIter iter;
    gboolean b;
    gchar *key, *value;

    macro->resetContent();

    tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(dialog), "tree_macro"));
    model = GTK_TREE_MODEL(gtk_tree_view_get_model(tree));

    b = gtk_tree_model_get_iter_first(model, &iter);
    while (b == TRUE)
    {
        gtk_tree_model_get(model, &iter, COL_KEY, &key, COL_VALUE, &value, -1);

        if (strcasecmp(key, STR_NULL_ITEM) != 0)
        {
            macro->addItem(key, value, CONV_CHARSET_XUTF8);
        }

        b = gtk_tree_model_iter_next(model, &iter);
    }
}

void check_last_macro_in_list(GtkListStore* list)
{
    GtkTreeIter iter;
    gchar *key;
    gint n;

    // get number item in list
    n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL);

    if (n > 0)
    {
        // get last item
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &iter, NULL, n-1);

        // get key of item
        gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, COL_KEY, &key, -1);

        // if key is value used for NULL item
        if (strcmp(key, STR_NULL_ITEM) == 0)
        {
            return;
        }
    }

    // if last item is valid item or no item in list, add new NULL item
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       COL_KEY, STR_NULL_ITEM,
                       COL_VALUE, STR_NULL_ITEM,
                       -1);
}

void key_edited_cb (GtkCellRendererText *celltext,
                    const gchar *string_path,
                    const gchar *newkey,
                    gpointer data)
{
    GtkTreeView *tree;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *key, *oldkey, *oldvalue;
    gchar nkey[MAX_MACRO_KEY_LEN];
    gboolean b;

    tree = GTK_TREE_VIEW(data);
    model = gtk_tree_view_get_model(tree);

    strncpy(nkey, newkey, MAX_MACRO_KEY_LEN-1);
    nkey[MAX_MACRO_KEY_LEN-1] = '\0';

    if (strcmp(nkey, STR_NULL_ITEM) == 0
        || (strlen(STR_NULL_ITEM) != 0 && strlen(nkey) == 0))
    {
        return;
    }

    // check if any key same as newkey
    b = gtk_tree_model_get_iter_first(model, &iter);
    while (b)
    {
        gtk_tree_model_get(model, &iter, COL_KEY, &key, -1);
        if (strcasecmp(key, nkey) == 0)
        {
            return;
        }

        b = gtk_tree_model_iter_next(model, &iter);
    }
    // end check

    // get iter of newkey
    gtk_tree_model_get_iter_from_string(model, &iter, string_path);
    // get old value of that iter
    gtk_tree_model_get(model, &iter, COL_KEY, &oldkey, COL_VALUE, &oldvalue, -1);

    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_KEY, nkey, -1);

    if (strcmp(oldkey, STR_NULL_ITEM) == 0)
    {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_VALUE, MACRO_DEFAULT_VALUE);
    }

    check_last_macro_in_list(GTK_LIST_STORE(model));
}

void value_edited_cb(GtkCellRendererText *celltext,
                     const gchar *string_path,
                     const gchar *newvalue,
                     gpointer data)
{
    GtkTreeView *tree;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *key;
    gchar value[MAX_MACRO_TEXT_LEN];

    tree = GTK_TREE_VIEW(data);
    model = gtk_tree_view_get_model(tree);

    gtk_tree_model_get_iter_from_string(model, &iter, string_path);

    gtk_tree_model_get(model, &iter, COL_KEY, &key, -1);

    strncpy(value, newvalue, MAX_MACRO_TEXT_LEN-1);
    value[MAX_MACRO_TEXT_LEN-1] = '\0';

    if (strcmp(key, STR_NULL_ITEM) != 0)
    {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_VALUE, value, -1);
    }
}

void remove_macro_clicked_cb(GtkButton *button, gpointer user_data)
{
    GtkTreeView     *treeview;
    GtkListStore    *list;
    GtkTreeSelection*select;
    GtkTreeIter     iter;
    gchar           *key;

    treeview = GTK_TREE_VIEW(user_data);

    select = gtk_tree_view_get_selection(treeview);

    list = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));

    if (gtk_tree_selection_get_selected(select, NULL, &iter) == TRUE)
    {
        gtk_tree_model_get(GTK_TREE_MODEL(list), &iter, COL_KEY, &key, -1);

        if (strcmp(key, STR_NULL_ITEM) != 0)
        {
            gtk_list_store_remove(list, &iter);
        }

        gtk_tree_selection_select_iter(select, &iter); // select current index
    }
}

void removeall_macro_clicked_cb(GtkButton *button, gpointer data)
{
    GtkTreeView* tree;
    GtkListStore* list;
    GtkTreeIter iter;
    GtkTreeSelection* select;

    tree = GTK_TREE_VIEW(data);
    list = GTK_LIST_STORE(gtk_tree_view_get_model(tree));

    gtk_list_store_clear(list);

    check_last_macro_in_list(list);

    select = gtk_tree_view_get_selection(tree);

    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter);

    gtk_tree_selection_select_iter(select, &iter);
}

void import_macro_clicked_cb(GtkButton *button, gpointer data)
{
    GtkWidget* file;
    GtkTreeView* tree;
    GtkListStore* list;
    GtkTreeIter iter;
    CMacroTable macro;
    gchar* fn;
    int r, n;

    file = gtk_file_chooser_dialog_new(_("Choose file to import"),
                                       GTK_WINDOW(data),
                                       GTK_FILE_CHOOSER_ACTION_OPEN,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                       NULL);

    gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);

    r = gtk_dialog_run(GTK_DIALOG(file));
    if (r == GTK_RESPONSE_OK)
    {
        fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        macro.init();
        macro.loadFromFile(fn);
        g_free(fn);

        tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(data), "tree_macro"));
        list = GTK_LIST_STORE(gtk_tree_view_get_model(tree));

        n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list), NULL);     // get number of iter
        gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list), &iter, NULL, n-1); // get last iter
        gtk_list_store_remove(list, &iter); // remove last iter (...)

        append_macro_to_list_store(list, macro);

        check_last_macro_in_list(list); // add iter (...)

        // select first iter
        GtkTreeSelection* select = gtk_tree_view_get_selection(tree);
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &iter);
        gtk_tree_selection_select_iter(select, &iter);
    }

    gtk_widget_destroy(file);

    gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
}

void export_macro_clicked_cb(GtkButton *button, gpointer data)
{
    GtkWidget* file;
    GtkTreeView* tree;
    GtkTreeModel* model;
    GtkTreeIter iter;
    CMacroTable macro;
    gchar* key, *value;
    gchar* fn;
    int r;

    file = gtk_file_chooser_dialog_new(_("Choose file to export"),
                                       GTK_WINDOW(data),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                       NULL);

    gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);

    r = gtk_dialog_run(GTK_DIALOG(file));
    if (r == GTK_RESPONSE_OK)
    {
        macro.init();

        tree = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(data), "tree_macro"));
        model = GTK_TREE_MODEL(gtk_tree_view_get_model(tree));

        gboolean b = gtk_tree_model_get_iter_first(model, &iter);
        while (b == TRUE)
        {
            gtk_tree_model_get(model, &iter, COL_KEY, &key, COL_VALUE, &value, -1);

            if (strcasecmp(key, STR_NULL_ITEM) != 0)
            {
                macro.addItem(key, value, CONV_CHARSET_XUTF8);
            }
            b = gtk_tree_model_iter_next(model, &iter);
        }

        fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file));
        macro.writeToFile(fn);
        g_free(fn);
    }

    gtk_widget_destroy(file);

    gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
}

