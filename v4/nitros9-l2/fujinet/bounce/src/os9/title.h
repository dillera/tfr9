#ifndef OS9_TITLE_H
#define OS9_TITLE_H

// NitrOS-9's screens before the world (title.c), which get_info.c,
// connection.c, shapes.c and press_key.c use in place of their own.

void show_header(void);
void show_server(char *s);
void show_name(char *s);
void show_menu(void);
void get_info_changes(void);

void show_connecting(void);
void shapes_heading(void);
void show_shape_gallery(void);
void show_client_id(void);
void press_key_prompt(void);

void display_shape_data(uint8_t n, uint8_t x, uint8_t y);  // shapes.c

#endif // OS9_TITLE_H
