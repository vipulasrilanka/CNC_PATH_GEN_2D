/*
include.h file
Auther: Vipula Liyanaarachchi
*/

#define GLOBAL_BUFFLEN 1024

#define TAG_GEOMETRY_START "<!START GEOMETRY_CUT>"
#define TAG_PATH "<path"
#define TAG_PATH_VECTOR_START "d="
#define TAG_PEN_WIDTH "stroke-width=\""
#define TAG_PEN_COLOR "stroke=\""
#define PARAM_VALUE_END_CHAR '\"'

#define CMD_MOVE_TYPE_GO "GOTO"
#define CMD_MOVE_TYPE_LINE "LINE"
#define CMD_MOVE_TYPE_ARC_CLOCKWISE "ARC_CW"
#define CMD_MOVE_TYPE_ARC_COUNTER_CLOCKWISE "ARC_CCW"
#define CMD_MOVE_TYPE_CIRCLE_CLOCKWISE "CIRCLE_CW"
#define CMD_MOVE_TYPE_CIRCLE_COUNTER_CLOCKWISE "CIRCLE_CCW"
#define CMD_COMMAND_SEPERTOR ' '
#define CMD_NUMBER_SEPERATOR ','


enum command_pass_cycle {
  command_pass_error = -1,
  command_pass_init = 0,
  command_pass_type_found,
  command_pass_number_found,
  command_pass_number_end,
  command_pass_end_of_first_command,
  command_pass_start_end_of_path,
  command_pass_end_of_command_line
};


enum string_match_type {
  string_match_exact=1, //so that NULL is an invalid parameter.
  string_match_exact_less_leading_space,
  string_match_exact_ignore_leading_mismatch,
  string_match_part
};


enum error_codes {
  error_success,
  error_function_not_implemented,
  error_in_move,
  error_other,
  error_insufficient_argumemts,
  error_unknown,
  error_buffer_mismatch,
  error_bad_command_in_file,
  error_delemeter_in_file,
  error_tag_not_found_in_file,
  error_file_open
};

//function prototypes

int find_global_tag(FILE *fp, char *tag);
int check_tag(char * buff, char *tag, int *match_offset, int is_full_string);
int decode_path(FILE *fp, char *command);
int get_parameter_value (FILE *fp, char *parameter_string, char *param_value);
