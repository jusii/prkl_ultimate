#include "userinterface.h"
#include <stdlib.h>
#include <string.h>
#include "mystring.h" // my string class
#include "assembly_search.h"
#include "assembly_entry.h"
#include "assembly.h"
#include "network_config.h"


#define DEFAULT_HOSTNAME_ASS  "hackerswithstyle.se"
#define DEFAULT_HOSTNAME_COM  "commoserve.files.commodore.net"
#define DEFAULT_HOSTPORT      80
#define DEFAULT_URL_SEARCH_ASS "/leet/search/aql/0/100?query="
//#define DEFAULT_URL_SEARCH_COM "/leet/search/aql?query="
#define DEFAULT_URL_SEARCH_COM "/leet/search/aql/0/100?query="
#define DEFAULT_URL_PATTERNS  "/leet/search/aql/presets"
#define DEFAULT_URL_ENTRIES   "/leet/search/entries"
#define DEFAULT_URL_DOWNLOAD  "/leet/search/bin"
#define DEFAULT_CLIENTID_ASS  "Spiffy"
#define DEFAULT_CLIENTID_COM  "Commodore"

/****************************/
/* AssemblySearch UI Object */
/****************************/
/*
 * The first screen consists of a series of query fields.
 * Each of these fields are single line, thus a short description on the left
 * with a string on the right. There are two types of entries; the string
 * edit fields, and the drop down fields. For the drop down fields, the
 * context menus are used, just like in the config browser.
 * Some unselectable lines can be used for spacing.
 * A special third type of entry exits the screen to perform the search.
 * - BrowsableQueryField, with subtype: string entry, and select (drop down)
 * 
 * The second screen shows the results of the search. This screen will be 
 * populated with the 'entries'. These entries show the name and group,
 * maybe year of the release. These entries are a special type of
 * Browsable; which holds the reference to the ID and Category number
 * as listed on the Assembly server. When entering on such entry, the
 * downloadable items are fetched and shown on the third screen.
 * 
 * - BrowsableAssemblyEntry (second screen)
 * - BrowsableAssemblyItem (third screen), which may be a derivative of
 *      BrowsableDirEntry, such that the filetypes would work. However,
 *      the file must still be downloaded to the cache, and the path
 *      reference must be set to the cache for the commands to work. 
 */


AssemblySearch :: AssemblySearch(UserInterface *ui, Browsable *root) : TreeBrowser(ui, root)
{
    setCleanup();
    state = new AssemblySearchForm(root, this, 0);
    state->reload();
}

AssemblySearch :: ~AssemblySearch()
{
    printf("And there goes our Assembly browser!"); 
}

void AssemblySearch :: init(Screen *screen, Keyboard *k) // call on root!
{
	this->screen = screen;
	window = new Window(screen, (screen->get_size_x() - 40) >> 1, 2, 40, screen->get_size_y()-3);
	window->draw_border();
	keyb = k;
	state->do_refresh();
}

// Using the base class function deinit, which destroys the window.

static const char *queryhelp = 
        "1. Query Screen:\n"
        "\n"
		"CRSR UP/DN: Select field\n"
		"CLEAR:      Clear all fields\n"
        "DEL:        Clear selected field\n"
        "RETURN:     Field: Edit\n"
        "            Search: Send Query\n"
        "+/-:        Change preset option.\n"
        "RUN/STOP:   Close Search\n"
        "CRSR LEFT:  Close Search\n"
        "\n"
		"Quick type: Use the keyboard to type\n"
		"            directly in current\n"
        "            field.\n"
        "\n"
        "2. Query Result Screen:\n"
		"CRSR UP/DN: Select title\n"
        "RETURN/->:  View Result entries\n"
        "CRSR LEFT:  Return to Query screen\n"
        "\n"
        "3. Result Entries:\n"
        "\nWorks like any directory. Please\n"
        "note that accessing disks or files\n"
        "introduces a delay as the data needs\n"
        "to be downloaded from the internet.";

int AssemblySearch :: handle_key(int c)
{
    int ret = 0;
    
    if ((c == KEY_BREAK) || (c == KEY_ESCAPE) || (c == '`')) {
        return MENU_CLOSE; // independent of level, it closes the search.
        // if we'd have this handled by the tree browser, it would cause a HIDE instead
    }
    // For level 1 it's just like tree browser, with the exception of the return key / space
    // This could also be handled by the tree browser, if we check for context menus and do 'into' when none exist.
    if (((c == KEY_RETURN) || (c == KEY_SPACE)) && (state->level == 1)) {
        state->into();
        return 0;
    }
    if (state->level >= 1) {
        return TreeBrowser :: handle_key(c);
    }
    switch(c) {
        case KEY_F8: // exit
            ret = MENU_EXIT;
            break;
        case KEY_DOWN: // down
            state->down(1);
            break;
        case KEY_UP: // up
            state->up(1);
            break;
        case KEY_PAGEUP:
            state->up(window->get_size_y()/2);
            break;
        case KEY_PAGEDOWN:
            state->down(window->get_size_y()/2);
            break;
        case KEY_TASKS:
            ret = MENU_CLOSE; // do nothing in the non-commodore mode
            break;
        case KEY_HELP: 
            state->refresh = true;
            user_interface->run_editor(queryhelp, strlen(queryhelp));
            break;
        case KEY_CLEAR: //
            if(state->level == 0) {
                state->reload();
                state->do_refresh();
            }
            break;
        case KEY_HOME: // clear entry
            ((AssemblySearchForm *)state)->clear_entry();
            break;
        case KEY_SPACE: // space = select
        	state->select_one();
            break;
        case KEY_RETURN: // CR = select
            switch (state->level) {
            case 0:
                state->change();
                break;
            case 1:
                state->into();
                break;
            case 2:
                context(0);
                break;
            default:
                break;
            }
            break;
        case KEY_RIGHT: // right
            if(state->level!=0)
                state->into();
            break;
        case '+':
            if(state->level==0)
                state->increase();
            break;
        case '-':
            if(state->level==0)
                state->decrease();
            break;
        case KEY_LEFT: // left
		case KEY_BACK: // del
            if(state->level==0) {
                ret = MENU_CLOSE; // leave
            } else {
                state->level_up();
            }
            break;

        default:
            if ((state->level == 0) && (
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9'))) {
                keyb->push_head(c);
                state->change();
            } else {
                printf("Unhandled key: %b\n", c);
            }
    }    
    return ret;
}


AssemblySearchForm :: AssemblySearchForm(Browsable *node, TreeBrowser *tb, int level) : TreeBrowserState(node, tb, level)
{
    //default_color = 7;
}

AssemblySearchForm :: ~AssemblySearchForm()
{

}


void AssemblySearchForm :: send_query(void)
{
    mstring query;
    for(int i=0;i<children->get_elements();i++) {
        Browsable *b = (*children)[i];
        if (b->isSelectable()) {
            BrowsableQueryField *field = (BrowsableQueryField *)b;
            const char *name = field->getName();
            const char *value = field->getAqlString();
            if (strlen(value) == 0) {
                continue;
            }
            if (name[0] == '$') {
                continue;
            }
            if (query.length() > 0) {
                query += " & ";
            }
            query += "(";
            query += field->getName();
            query += ":";
            if (!field->isDropDown()) {
                query += "\"";
            }
            if ((strcasecmp(name, "rating") == 0) && (value[0] != '>')) {
                query += ">=";
            }
            query += value;
            if (!field->isDropDown()) {
                query += "\"";
            }
            query += ")";
        }
    }
    if (!query.length()) {
        browser->user_interface->popup("Queries cannot be empty!", BUTTON_OK);
        return;
    }
    printf("Query:\n%s\n", query.c_str());

    // Let the user know we are busy
    browser->window->set_color(6);
    browser->window->set_background(0);
    browser->window->getScreen()->move_cursor(0, browser->window->getScreen()->get_size_y()-1);
    browser->window->getScreen()->output_fixed_length("Sending query...", 0, browser->window->getScreen()->get_size_x()-9);

    JSON *response = assembly.send_query(query.c_str());
    // if (response) {
    //     puts(response->render());
    // }
    if (response) {
        if (response->type() == eList) {
            printf("Creating results view...\n");
            BrowsableQueryResults *rb = new BrowsableQueryResults((JSON_List *)response, browser->window->get_size_x());
            deeper = new AssemblyResultsView(rb, browser, 1);
            deeper->previous = this;
            int error;
        	deeper->children = rb->getSubItems(error);
        	int child_count = deeper->children->get_elements();
            printf("Number of results: %d.\n", child_count);
            browser->state = deeper;
        }
        delete response;
    } else {
        browser->window->set_color(10);
        browser->window->set_background(0);
        browser->window->getScreen()->move_cursor(0, browser->window->getScreen()->get_size_y()-1);
        browser->window->getScreen()->output_fixed_length("** Connection FAILED **", 0, browser->window->getScreen()->get_size_x()-9);
    }
    t_BufferedBody *body = (t_BufferedBody *)assembly.get_user_context();
    if (body)
        delete body;
}

void AssemblySearchForm :: change(void)
{
	if(!under_cursor)
		return;

    BrowsableQueryField *field = (BrowsableQueryField *)under_cursor;

    char buffer[32];
    if ((field->getName())[0] == '$') {  // The dirtiest trick ever!
        send_query();
    } else if (field->isDropDown()) {
        browser->context(0);
        // refresh will take place, because the context menu disappears and refresh flag is set
    } else {
        strcpy(buffer, field->getStringValue());
        browser->window->set_color(1);
        browser->user_interface->string_edit(buffer, 26, browser->window, 10, this->cursor_pos);
        field->setStringValue(buffer);
        // explicit refresh
        refresh = true;
        down(1);
    }
}

void AssemblySearchForm :: increase(void)
{
    if(!under_cursor)
        return;

    BrowsableQueryField *field = (BrowsableQueryField *)under_cursor;
    field->updown(1);
    update_selected();
}

void AssemblySearchForm :: decrease(void)
{
    if(!under_cursor)
        return;

    BrowsableQueryField *field = (BrowsableQueryField *)under_cursor;
    field->updown(-1);
    update_selected();
}

void AssemblySearchForm :: clear_entry(void)
{
    if(!under_cursor)
        return;

    BrowsableQueryField *field = (BrowsableQueryField *)under_cursor;
    field->reset();
    update_selected();
}

AssemblyResultsView :: AssemblyResultsView(Browsable *node, TreeBrowser *tb, int level) : TreeBrowserState(node, tb, level)
{
    //default_color = 7;
}

AssemblyResultsView :: ~AssemblyResultsView()
{
}

void AssemblyResultsView :: into()
{
    BrowsableQueryResult *item = (BrowsableQueryResult *)under_cursor;
    if (!item)
        return;

    deeper = new TreeBrowserState(item, browser, 2);
    deeper->previous = this;
    int error;
    deeper->children = item->getSubItems(error);
    browser->state = deeper;
    char cat[16];
    browser->path->cd("/a64");
    browser->path->cd(item->getId());
    sprintf(cat, "%d", item->getCategory());
    browser->path->cd(cat);
    int child_count = deeper->children->get_elements();
    printf("Number of entries: %d. Path = %s\n", child_count, browser->getPath());
}

void AssemblyResultsView :: get_entries()
{

}

void AssemblyResultsView :: move_to_index(int idx)
{
    TreeBrowserState::move_to_index(idx);
    show_status();
}

void AssemblyResultsView :: draw()
{
    TreeBrowserState::draw();
    show_status();
}

void AssemblyResultsView :: show_status()
{
    BrowsableQueryResult *item = (BrowsableQueryResult *)under_cursor;
    if (!item)
        return;
    // show status line
    browser->window->reverse_mode(0);
    browser->window->set_color(13);//browser->user_interface->color_fg);
    browser->window->set_background(browser->user_interface->color_bg);
    browser->window->getScreen()->move_cursor(0, browser->window->getScreen()->get_size_y()-1);
    mstring infostr;
    infostr += item->getUpdated();
    infostr += " ";
    int y = item->getYear();
    if ((y>=1000)&&(y<=9999))
        infostr += y;
    else
        infostr += "    ";
    infostr += " cat:";
    infostr += item->getCategory();
    browser->window->getScreen()->output_fixed_length(infostr.c_str(), 0, browser->window->getScreen()->get_size_x()-9);
}

void BrowsableAssemblyRoot :: fetchPresets(void)
{
    presets = assembly.get_presets(server_data);
}

IndexedList<Browsable *> *BrowsableAssemblyRoot::getSubItems(int &error)
{
    // name, group, handle, event, date*, category*, subcat*, rating*, type*, repo*, latest, sort, order
    if (children.get_elements() == 0) {
            children.append(new BrowsableStatic(server_data->title.c_str()));
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableQueryField("name", NULL));
            children.append(new BrowsableQueryField("group", NULL));
            children.append(new BrowsableQueryField("handle", NULL));
            children.append(new BrowsableQueryField("event", NULL));
            if (presets && (presets->type() == eList)) {
                JSON_List *list = (JSON_List *)presets;
                for (int i = 0; i < list->get_num_elements(); i++) {
                    JSON_Object *obj = (JSON_Object *)(*list)[i];
                    JSON *type = obj->get("type");
                    JSON *values = obj->get("values");
                    if (type && type->type() == eString && values && values->type() == eList) {
                        children.append(new BrowsableQueryField(((JSON_String *)type)->get_string(), (JSON_List *)values));
                    }
                }
            }
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableQueryField("$", NULL));

          if (children.get_elements() < 16) {
            children.append(new BrowsableStatic(""));
            children.append(new BrowsableStatic("You agree you have a necessary license"));
            children.append(new BrowsableStatic("or rights to download any software."));
          }
    }
    return &children;
}

#include "browsable_root.h"
IndexedList<Browsable *> *BrowsableQueryResult :: getSubItems(int &error)
{
    // name, group, handle, event, date*, category*, subcat*, rating*, type*, repo*, latest, sort, order
    error = 0;
    if (children.get_elements() == 0) {
        JSON *j = assembly.request_entries(id.c_str(), category);
        if (j && j->type() == eObject) {
            JSON *content = ((JSON_Object *)j)->get("contentEntry");
            if (content && content->type() == eList) {
                JSON_List *list = (JSON_List *)content;
                for(int i=0; i < list->get_num_elements(); i++) {
                    JSON *el = (*list)[i];
                    if (el->type() == eObject) {
                        children.append(new BrowsableDirEntryAssembly(this, (JSON_Object *)el, id.c_str(), category));
                    }
                }
            }
        } else {
            error = 1;
        }
    }
    return &children;
}

BrowsableQueryResult :: BrowsableQueryResult(JSON_Object *result, int window_width) : path("/a64")
{
    JSON *j;
    year = 0;

    j = result->get("name");
    if (j && j->type() == eString) {
        summary = ((JSON_String *)j)->get_string();
    }
    j = result->get("year");
    if (j && j->type() == eInteger) {
        year = ((JSON_Integer *)j)->get_value();
    }
    j = result->get("group");
    if (j && j->type() == eString) {
        summary += " ";
        summary2 += "\e\x0d";
        summary2 += ((JSON_String *)j)->get_string();
        int l = summary.length()+summary2.length()-2; // subtract 2 characters for escape codes
        if (year && (l+5<=window_width)) {
            summary2 += ",";
            summary2 += year;
        }
        l = summary.length()+summary2.length()-2; // subtract 2 characters for escape codes
        while (l++ < window_width)
            summary += " ";
    }
    category = 0;
    j = result->get("category");
    if (j && j->type() == eInteger) {
        category = ((JSON_Integer *)j)->get_value();
    }
    j = result->get("id");
    if (j && j->type() == eString) {
        id = ((JSON_String *)j)->get_string();
    }
    j = result->get("updated");
    if (j && j->type() == eString) {
        updated = ((JSON_String *)j)->get_string();
    }
    path.cd(id.c_str());
    char catstr[8];
    sprintf(catstr, "%d", category);
    path.cd(catstr);
}

void BrowsableQueryResult :: getDisplayString(char *buffer, int width)
{
    bool extra = (summary.length()<width);
    if (extra)
        width += 2;
    memset(buffer, ' ', width);
    buffer[width] = '\0';

    strncpy(buffer, summary.c_str(), width);
    if (extra)
    {
        strncpy(&buffer[summary.length()], summary2.c_str(), width-summary.length());
    }
}

AssemblyInGui::AssemblyInGui() {
    root = NULL;
    dbselection = 0;
    server_count = 0;

    taskItemCategory = TasksCollection :: getCategory("Assembly 64", SORT_ORDER_ASSEMBLY);
}

#define JSON_GET(j, name) \
    ((j && (j->type() == eObject)) ? ((JSON_Object*) j)->get(name) : NULL)

#define JSON_GET_STRING(obj, name, s) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eString)) \
        s = strdup(((JSON_String*) j)->get_string());

#define JSON_GET_INTEGER(obj, name, v) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eInteger)) \
        v = ((JSON_Integer*) j)->get_value();

int AssemblyInGui::load_custom()
{
#ifndef RECOVERYAPP
    const char* branding_directory = "/Flash/config";

    ConfigManager *cm = ConfigManager :: getConfigManager();
    if (cm->get_safe_mode())
        return 0;

    FileManager *fm = FileManager::getFileManager();
    size_t bufferSize = 4096;
    char* jsonText = (char*) malloc(bufferSize);
    if (!jsonText)
        return 0;

    uint32_t jsonTextSize=0;
    JSON *obj = NULL;
    FRESULT fres = fm->load_file(branding_directory, "server.json", (uint8_t*) jsonText, bufferSize, &jsonTextSize);

    if ((fres != 0)||(jsonTextSize == 0)) {
        free(jsonText);
        return 0;
    }

    server_count = 0;
    printf("Loading search.json...\n");
    convert_text_to_json_objects(jsonText, (size_t) jsonTextSize, 512, &obj);
    do {
        if (!obj) {
            printf("Invalid JSON data!\n");
            break;
        }

        printf("Evaluating search.json file...\n");
        JSON* j = JSON_GET(obj, "assembly64");
        JSON_List* l = (j && (j->type() == eList)) ? (JSON_List*) j : NULL;
        if (!l)
            break;

        int max_count = l->get_num_elements();
        if (max_count < 1)
            break;
        if (max_count > 8)
            max_count = 8;

        ServerList = new SearchService*[max_count];
        if (!ServerList)
            break;

        for (int i=0;i < max_count;i++) {
            JSON* e = (*l)[i];
            if (e && (e->type() == eObject)) {
                // set useful defaults
                const char* name      = "no name";
                const char* host      = DEFAULT_HOSTNAME_ASS;
                int         port      = DEFAULT_HOSTPORT;
                const char* client_id = DEFAULT_CLIENTID_ASS;
                const char* usearch   = DEFAULT_URL_SEARCH_ASS;
                const char* upatterns = DEFAULT_URL_PATTERNS;
                const char* uentries  = DEFAULT_URL_ENTRIES;
                const char* udownload = DEFAULT_URL_DOWNLOAD;

                JSON_GET_STRING(e, "name",         name);
                JSON_GET_STRING(e, "host",         host);
                JSON_GET_INTEGER(e,"port",         port);
                JSON_GET_STRING(e, "client-id",    client_id);
                JSON_GET_STRING(e, "url-search",   usearch);
                JSON_GET_STRING(e, "url-patterns", upatterns);
                JSON_GET_STRING(e, "url-entries",  uentries);
                JSON_GET_STRING(e, "url-download", udownload);

                ServerList[server_count++] = new SearchService(
                    name, host, port, client_id,
                    usearch, upatterns, uentries, udownload);
            }
        }
    } while (0);

    delete obj;
    free(jsonText);
#endif
    return server_count;
}

int AssemblyInGui::get_servers()
{
    if ((!ServerList)||(!server_count))
        server_count = load_custom();

    if ((!ServerList)||(!server_count)) {
        ServerList = new SearchService*[2];
        server_count = 0;
        ServerList[server_count++] =
            new SearchService("Assembly 64",
                              DEFAULT_HOSTNAME_ASS, DEFAULT_HOSTPORT,
                              DEFAULT_CLIENTID_ASS,
                              DEFAULT_URL_SEARCH_ASS,
                              DEFAULT_URL_PATTERNS,
                              DEFAULT_URL_ENTRIES,
                              DEFAULT_URL_DOWNLOAD);

        ServerList[server_count++] =
            new SearchService("CommoServe",
                              DEFAULT_HOSTNAME_COM, DEFAULT_HOSTPORT,
                              DEFAULT_CLIENTID_COM,
                              DEFAULT_URL_SEARCH_COM,
                              DEFAULT_URL_PATTERNS,
                              DEFAULT_URL_ENTRIES,
                              DEFAULT_URL_DOWNLOAD);
    }

    if (!ServerNameList) {
        printf("search service server:\n");
        ServerNameList = new const char*[server_count+1];
        for (int i=0;i<server_count;i++) {
            ServerNameList[i] = ServerList[i]->name;
            printf("server %i: %s\n", i, ServerNameList[i]);
        }
    }

    return server_count;
}

const char** AssemblyInGui::ServerNameList = NULL;
SearchService** AssemblyInGui::ServerList = NULL;
int AssemblyInGui::server_count = 0;

AssemblyInGui assembly_gui;
