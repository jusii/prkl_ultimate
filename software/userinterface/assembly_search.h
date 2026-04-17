#ifndef ASSEMBLY_SEARCH_H
#define ASSEMBLY_SEARCH_H

#include "tree_browser.h"
#include "tree_browser_state.h"
#include "context_menu.h"
#include "subsys.h"
#include "json.h"
#include "menu.h"
#include "action.h"
#include "network_interface.h"

class SearchService {
public:
    const char* name;
    mstring     title;
    const char* host;
    uint16_t    port;
    const char* client_id;
    const char* url_search;
    const char* url_patterns;
    const char* url_entries;
    const char* url_download;

    SearchService(const char* _name, const char* _host, uint16_t _port,
                  const char* _client_id, const char* _usearch,
                  const char* _upatterns, const char* _uentries,
                  const char* _udownload)
    {
        name = strdup(_name);
        title += "\em  ";
        title += name;
        title += " Search";

        host = strdup(_host);
        port = _port;
        client_id    = strdup(_client_id);
        url_search   = strdup(_usearch);
        url_patterns = strdup(_upatterns);
        url_entries  = strdup(_uentries);
        url_download = strdup(_udownload);
    }
};

class AssemblySearchForm: public TreeBrowserState
{
    void send_query(void);
public:
    AssemblySearchForm(Browsable *node, TreeBrowser *tb, int level);
    ~AssemblySearchForm();

    void into(void) { printf("Search Form Into\n"); }
    void level_up(void) { printf("Search Form Up\n"); };
    void change(void);
    void increase(void);
    void decrease(void);
    void clear_entry(void);
};

class AssemblyResultsView: public TreeBrowserState
{
    void get_entries(void);
public:
    AssemblyResultsView(Browsable *node, TreeBrowser *tb, int level);
    ~AssemblyResultsView();

    void into(void);
    bool into2(void) { into(); return true; }
//    void level_up(void) { printf("Results Up\n"); };

    //virtual void draw_item(Browsable *t, int line, bool selected);
    void show_status();
    virtual void move_to_index(int idx);
    virtual void draw();
};

class AssemblySearch: public TreeBrowser
{
public:
    AssemblySearch(UserInterface *ui, Browsable *);
    virtual ~AssemblySearch();

    void init(Screen *screen, Keyboard *k);
    int handle_key(int);
    void checkFileManagerEvent(void)
    {
    } // we are not listening to file manager events
};

class BrowsableQueryField: public Browsable
{
    const char *field;
    JSON_List *presets; // When NULL, use a string edit.     
    mstring value;
    int current_preset;

    static SubsysResultCode_e update(SubsysCommand *cmd)
    {
        BrowsableQueryField *field = (BrowsableQueryField *)cmd->functionID;
        // field->value = cmd->actionName;
        field->current_preset = cmd->mode;
        field->setPreset();
        return SSRET_OK;
    }
public:
    BrowsableQueryField(const char *field, JSON_List *presets) : field(field), presets(presets)
    {
        current_preset = -1;
    }

    ~BrowsableQueryField()
    {
    }

    const char *getName()
    {
        return field;
    }

    void reset()
    {
        current_preset = -1;
        value = "";
    }

    const char *getAqlString()
    {
        if (presets) {
            if (current_preset == -1) {
                return "";
            }
            JSON_Object *obj = (JSON_Object *)(*presets)[current_preset];
            JSON_String *aql = (JSON_String *)obj->get("aqlKey");
            if (aql) {
                return aql->get_string();
            }
        }
        return value.c_str();
    }

    const char *getStringValue()
    {
        return value.c_str();
    }

    void setStringValue(const char *s)
    {
        value = s;
    }

    bool isDropDown(void)
    {
        return (presets != NULL);
    }

    void updown(int offset)
    {
        if (!presets)
            return;

        current_preset += offset;
        if (current_preset < 0)
            current_preset = 0;
        if (current_preset >= presets->get_num_elements()) {
            current_preset = presets->get_num_elements() - 1;
        }
        setPreset();
    }

    void setPreset()
    {
        JSON *el = (*presets)[current_preset];
        if (el->type() != eObject) {
            return;
        }
        JSON_Object *obj = (JSON_Object *)el;
        JSON *value = obj->get("name");
        if (!value) {
            value = obj->get("aqlKey");
        }
        if (value->type() != eString) {
            return;
        }
        setStringValue(((JSON_String *)value)->get_string());
    }

    void getDisplayString(char *buffer, int width)
    {
        // minimum window size = 38
        // Field name: 8 chars max, colon, space = 10. Left: 28
        // Max field length = 26

        memset(buffer, ' ', width+2);
        buffer[width+2] = '\0';

        if (width < 16) {
            return;
        }

        if (field[0] == '$') {
            sprintf(buffer, "\er           \eR <<  Submit  >> \er");
            return;
        }

        sprintf(buffer, "%s:", field);
        buffer[strlen(buffer)] = ' ';
        if (width < 16) {
            return;
        }

        if (value.length()) {
            // position 10
            sprintf(buffer+10, "\er\eg%#s", width-11, value.c_str());
        } else {
            sprintf(buffer+10, "\er\ek%#s", width-11, "__________________");
        }
        if (buffer[0] > 0x60)
            buffer[0] &= 0xDF; // Capitalize ;-)
    }

    void fetch_context_items(IndexedList<Action *>&actions)
    {
        if (!presets) {
            return;
        }
        for (int i = 0; i < presets->get_num_elements(); i++) {
            JSON *el = (*presets)[i];
            if (el->type() != eObject) {
                continue;
            }
            JSON_Object *obj = (JSON_Object *)el;
            JSON *value = obj->get("name");
            if (!value) {
                value = obj->get("aqlKey");
            }
            if (value->type() != eString) {
                continue;
            }
            actions.append(new Action(((JSON_String *)value)->get_string(), update, (int)this, i));          
        }
    }

};

class BrowsableAssemblyRoot: public Browsable
{
    JSON *presets;
    SearchService* server_data;

    static SubsysResultCode_e new_search(SubsysCommand *cmd)
    {
        printf("Creating search menu...\n");
            
        Browsable *root = (Browsable *)cmd->functionID;
        AssemblySearch *searchBrowser = new AssemblySearch(cmd->user_interface, root);
        searchBrowser->init(cmd->user_interface->screen, cmd->user_interface->keyboard);
        cmd->user_interface->activate_uiobject(searchBrowser);
        // from this moment on, we loose focus.. polls will go directly to config menu!
        return SSRET_OK;
    }
public:
    BrowsableAssemblyRoot(SearchService* _server_data)
    {
        presets = NULL;
        server_data = _server_data;
    }

    ~BrowsableAssemblyRoot()
    {
    }

    void fetchPresets(void);
    bool isInitialized(void)
    {
        return (presets != NULL);
    }

    IndexedList<Browsable *> *getSubItems(int &error);

    void getDisplayString(char *buffer, int width) {
        sprintf(buffer, "A64     Assembly 64 Database");
    }

	void fetch_context_items(IndexedList<Action *>&items) {
        items.append(new Action("New Search..", new_search, (int)this, 0));
    }

    const char *getName()
    {
        return (server_data) ? server_data->title.c_str() : "?";
    }
};

class BrowsableQueryResult: public Browsable
{
    mstring summary;
    mstring summary2;
    //mstring name;
    //mstring group;
    //mstring year;
    mstring id;
    mstring updated;
    int category;
    int year;
    Path path;
public:
    BrowsableQueryResult(JSON_Object *result, int window_width);

    ~BrowsableQueryResult()
    {
    }

    const char *getId() { return id.c_str(); }
    const char *getUpdated() { return updated.c_str(); }
    int getCategory() { return category; }
    int getYear() { return year; }

    const char *getName()
    {
        return summary.c_str();
    }

    Path *getPath()
    {
        return &path;
    }

    void getDisplayString(char *buffer, int width);

    IndexedList<Browsable *> *getSubItems(int &error);

};

class BrowsableQueryResults : public Browsable // Root of results screen
{
    IndexedList<Browsable *>items; // Override from Browsable
public:
    BrowsableQueryResults(JSON_List *results, int window_width) : items(16, NULL)
    {
        for(int i=0; i<results->get_num_elements(); i++) {
            JSON *j = (*results)[i];
            if (j && j->type() == eObject) {
                JSON_Object *obj = (JSON_Object *)j;
                items.append(new BrowsableQueryResult(obj, window_width));
            }
        }
    }
    ~BrowsableQueryResults() { }

    IndexedList<Browsable *> *getSubItems(int &error)
    {
        return &items;
    }

};

class AssemblyInGui;
extern AssemblyInGui assembly_gui;

class AssemblyInGui : public ObjectWithMenu
{
    TaskCategory *taskItemCategory;
    BrowsableAssemblyRoot * root;
    int dbselection;
    static const char** ServerNameList;
    static SearchService** ServerList;
    static int server_count;

    static SubsysResultCode_e S_OpenSearch(SubsysCommand *cmd) {
        UserInterface *cmd_ui = cmd->user_interface;
        S_OpenSearch(cmd_ui);
        return SSRET_OK;
    }

    static int load_custom();
    static int get_servers();
   
public:
    AssemblyInGui();

    ~AssemblyInGui() {
        // unregister taskItemCategory
    }

    static void S_OpenSearch(UserInterface *cmd_ui) {

        if (!NetworkInterface :: DoWeHaveLink()) {
            if (cmd_ui)
                cmd_ui->popup("No Valid Network Link", BUTTON_OK);
            return;
        }

        Screen *scr = cmd_ui->screen;

        // TODO: Refactor to function of screen itself
        scr->set_color(6);
        scr->set_background(0);

        int selection = 0;
        if (cmd_ui) {
            int count = get_servers();
            if (count < 1)
                return;
            if (count > 1)
                selection = cmd_ui->choice("Internet File Search", ServerNameList, count);
            if (selection < 0)
                return;
        }

        scr->move_cursor(0, scr->get_size_y()-1);
        scr->output_fixed_length("Connecting...", 0, scr->get_size_x()-9);

        BrowsableAssemblyRoot *root = assembly_gui.getRoot(selection);
        if ((!root)||(!root->isInitialized())) {
            if (cmd_ui)
                cmd_ui->popup("Could not connect.", BUTTON_OK);
            return;
        }

        AssemblySearch *search_window = new AssemblySearch(cmd_ui, assembly_gui.getRoot(selection));
        search_window->init(cmd_ui->screen, cmd_ui->keyboard);
        search_window->setCleanup();
        cmd_ui->activate_uiobject(search_window); // now we have focus
    }

    BrowsableAssemblyRoot *getRoot(int selection)
    {
        if (dbselection != selection) {
            dbselection = selection;
            delete root;
            root = NULL;
        }

        if (!root) {
            root = new BrowsableAssemblyRoot(ServerList[selection]);
        }
        if (!root->isInitialized()) {
            root->fetchPresets();
        }
        return root;
    }

    void create_task_items(void) {
        // taskItemCategory->append(new Action("New Search", S_OpenSearch, 0, 0));
    }
};

#endif
