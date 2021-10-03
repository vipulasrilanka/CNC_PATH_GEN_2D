#include <stdio.h>
#include <math.h>
#include <string.h>
#include <geometry_gen.h>
#include <cnc_2d_common.h>


/*
The read file name is defined in main() for simplicity.
This was done so that we can move it to a comman line
argument later.
*/

int main(int argc, char *argv[]) {



  FILE *fp; //file pointer
  FILE *outfp;//output file pointer
  fpos_t position; //read position.
  int match_offset = 0;
  char command [GLOBAL_BUFFLEN]; //holds the last decoded command
  int param_value = 0; //Store the parameter values like tool/pen width


  /*
  argument processor
  */
  for (int arg_index = 0; arg_index < argc; arg_index++)
    printf("main(): argument# %d %s\n",arg_index, argv[arg_index] );

  if (argc < 2) {
    printf("main(); Usage error. Insufficient argumemts.\n");
    return(error_insufficient_argumemts);
  }

  //First Argument is the input html and the next is the output cmn.

  /* opening file for reading */
  fp = fopen(argv[1] , "r");
  if(NULL == fp ) {
     perror("main(): Error opening read file");
     return(-1);
  }

   outfp = fopen(argv[2], "w");
   if (NULL == outfp){
     perror("main(): Error opening write file");
     return(-1);
   }


   //Serch for TAG_GEOMETRY_START
   //This is where the
   printf("main(): Looking for %s\n", TAG_GEOMETRY_START );

   if(error_success != find_global_tag(fp, TAG_GEOMETRY_START)) {
     printf("main(): Could not find GLOBAL_GEOMETRY_START\n");
     fclose(fp);
     return(error_tag_not_found_in_file);
   }

   //Serch for TAG_PATH
   printf("main(): Looking for %s\n", TAG_PATH );

   if(error_success != find_global_tag(fp, TAG_PATH)) {
     printf("main(): Could not find TAG_PATH\n");
     fclose(fp);
     return(error_tag_not_found_in_file);
   }

   fgetpos(fp, &position); //Store the file locaton

   //Serch for TAG_PATH_VECTOR_START
   printf("main(): Looking for %s\n", TAG_PATH_VECTOR_START );
   if(error_success != find_global_tag(fp, TAG_PATH_VECTOR_START)) {
     printf("main(): Could not find TAG_PATH\n");
     fclose(fp);
     return(error_tag_not_found_in_file);
   }

   fgetpos(fp, &position); //Store the file locaton

   //go through all commands.
   while (error_success == decode_path(fp, command)) {
     printf("main(): command = %s\n",command );
     fputs(command, outfp);
     fputs("\r\n", outfp);
   }

   //printf("main(): get_parameter_value returned : %d and the value is %s \n",get_parameter_value(fp,TAG_PEN_COLOR,command), command);
  //Here we are trying to get the tool width, from the pen width param.
   if(error_success == get_parameter_value(fp,TAG_PEN_WIDTH,command)) {
     printf("Tool width =%s\n",command );
     fputs("TOOL F ",outfp);
     fputs(command, outfp);
     fputs("\r\n", outfp);
   }
   else{
     printf("main(): Could not find tool width\n");
     fclose(fp);
     return(error_tag_not_found_in_file);
   }

   //We need the tool width, which is the stroke-width in SVG
   //Serch for TAG_PEN_WIDTH
/*   printf("Looking for %s\n", TAG_PEN_WIDTH );
   if(error_success != find_global_tag(fp, TAG_PEN_WIDTH)) {
     printf("Could not find TAG_PEN_WIDTH\n");
     fclose(fp);
     return(error_tag_not_found_in_file);
   }
*/
   if(NULL != fgets (command, GLOBAL_BUFFLEN, fp)) {
     printf("main(): Next line string [%s]\n",command );
   }

   fclose(fp);
   fclose(outfp);

   return(error_success);
}


/*
get_parameter_value()
This function will exreact the valuse of parameters like
stroke-width="2", where the valuse is "2". Please note that there can be
any string, and it's not limited to numbers.
parameter_string should contain the parameter name, "stroke-width="
in the above case, including the = sign. This was done for simplicity.
*/

int get_parameter_value (FILE *fp, char *parameter_string, char *param_value) {

  char file_text_line[GLOBAL_BUFFLEN]; //buffer to read and store file
  int match_offset = 0; //holds the location of last match returned from chec_tag
  char read_byte=0;
  int index = 0;

  do {
     if(NULL != fgets (file_text_line, GLOBAL_BUFFLEN, fp)) {
        //Check for a tag match
        if(error_success==check_tag(file_text_line,parameter_string, &match_offset, string_match_exact_ignore_leading_mismatch)){
          printf("get_parameter_value: match at position %d of %zu long string '%s'",match_offset,strlen(file_text_line),file_text_line);
          if (file_text_line[match_offset] >= ' '){ //readable char, and not CR/LF
            //We have not reached the end of that line, and there are some text after
            //the match. We have to restore the file pointer to the end of the match.
            if (0 != fseek(fp,-(strlen(file_text_line)-(match_offset)),SEEK_CUR)) {
              printf("Error pointer correction: of string %s to -%zu\n",file_text_line, (strlen(file_text_line)-(match_offset)));
            } //move the file pointer back.
            else{
              printf("Restore File pointer as next char=> ASCII 0x%X CHAR '%c' back -%zu places\n", file_text_line[match_offset], file_text_line[match_offset], (strlen(file_text_line)-(match_offset)));
            }
            //Note - the file pointer increments when you .fgets()
          }
          else { //Not a readable char. We have reached to the end of
            printf("Next char is non readable=> ASCII 0x%X CHAR '%c' END\n", file_text_line[match_offset], file_text_line[match_offset]);
          }
          //Now we have found the param name, let's extract the value, that ends with a " charactor.
          do {
            read_byte = fgetc(fp); //read one byte.
            printf("Char read %c\n",read_byte);
            if (read_byte == EOF) {
                //we have reached EOF before
                return(error_delemeter_in_file);
            }
            else {
              if (read_byte == PARAM_VALUE_END_CHAR) {
                printf("Found end char %c \n", PARAM_VALUE_END_CHAR);
                param_value[index]= 0; //end the string
                return(error_success);
              }
              else {
                printf("add %c to return value\n", read_byte );
                param_value[index]= read_byte;
                index++;
              }//else (not " char)
            }//else (not EOF)
          }while (!feof(fp));
          //We should not be here.
          printf("We should not be here..");
          return(error_unknown);
        }//if tag match
        else {//tag does not match
          printf("get_parameter_value: miss match at position %d of %zu long string '%s'",match_offset,strlen(file_text_line),file_text_line);
        }
      }
  }while (!feof(fp));
  //We shoud not be here as we read till EOF is there is no param value end char
  //We are here as we did not get a match till EOF.
  //We do not reset the file pointer here.
  //The calling function is responsible to restore, when an error occurs.
  return(error_unknown);

}


/*
decode_path
This is to decode SVG path variables
First, the file pointer should be pointed to the start of the first
drawing instruction after the <path d=".
This function will detect a valid SVG path and convert it to a command.
For example, M 20 20 will be translated to GOTO 20,20.
This function will return success when a valid command is found. It will
incfement file read pointer to the end of the decoded command.
*/
int decode_path(FILE *fp, char *command) {

  char file_text_line[GLOBAL_BUFFLEN];
  int read_index = 0; //this is the point for read buffer
  int write_index = 0; //this is the point of write buffer
  int command_state_mc = command_pass_init;
  //a valid command consist of a charactoer and multiple numbers.
  //Here we assume one command does not extend beyond one line in file.

  if(NULL != fgets (file_text_line, GLOBAL_BUFFLEN, fp)) {
    printf("Serching string %s\n", file_text_line );
  }
  //we go in a loop through each and every charactor here.
  for (read_index = 0; read_index < strlen(file_text_line); read_index ++)
  {
    //printf("read_index %d %c\n", read_index, file_text_line[read_index]);
    switch (file_text_line[read_index]) {
      //White Space
      case ' ':
        //printf("White Space\n" );
        if (command_pass_number_found == command_state_mc) {
          //We hit space while trackig a number so this is the of it.
          //Add a comma here.
          command_state_mc = command_pass_number_end;
          //printf("STATE_NUMBER_END\n");
        }
        break;
        //The double quote
        case '\"':
        //printf("End/Start of path\n" );
          switch (command_state_mc) {
            //found " at the end of a number.
            case command_pass_number_found:
            case command_pass_number_end:
            case command_pass_init:
              command_state_mc = command_pass_start_end_of_path;
              //printf("STATE_START_PATH_END\n");
              break;
            //found " out of place
            default:
            command_state_mc = command_pass_error;
            //printf("STATE_COMMAND_ERROR\n");
            break;
          }
          break;

      //End of line
      case '\n':
      case '\r':
        //printf("LF or CR\n" );
        switch (command_state_mc) {
          case command_pass_number_end:
          case command_pass_number_found:
          case command_pass_start_end_of_path:
            //We are at the end of a number. so this is a command end.
            //Please note we do not support single command in multiple lines
            command_state_mc = command_pass_end_of_command_line;
            //printf("STATE_COMMAND_END\n");
          break;
          //out of place.
          default:
            //out of palce or premature termination of a command.
            //note we do not support commands in multiple liles.
            command_state_mc = command_pass_error;
            //printf("STATE_COMMAND_ERROR\n");
          break;

        }
        break;
      //Move Command
      case 'M':
        //printf("Command M\n" );
        switch (command_state_mc) {
          //First command
          case command_pass_init:
          case command_pass_start_end_of_path:
            command_state_mc = command_pass_type_found;
            //printf("STATE_COMMAND_FOUND\n");
            strcpy(command+write_index, CMD_MOVE_TYPE_GO);
            write_index += sizeof(CMD_MOVE_TYPE_GO) - 1;
            break;
          //Second Command.
          //We will not process it here.
          case command_pass_number_end:
            command_state_mc = command_pass_end_of_first_command;
            //printf("STATE_COMMAND_END\n");
            break;
          //We shold not be here.
          default:
            command_state_mc = command_pass_error;
          break;
        }//End switch command_state_mc in command M
        break;
      //Arc Command
      case 'A':
        //printf("Command A\n" );
        switch (command_state_mc) {
          //First command
          case command_pass_init:
          case command_pass_start_end_of_path:
            command_state_mc = command_pass_type_found;
            //printf("STATE_COMMAND_FOUND\n");
            strcpy(command+write_index, CMD_MOVE_TYPE_ARC_CLOCKWISE);
            write_index += sizeof(CMD_MOVE_TYPE_ARC_CLOCKWISE) - 1;
            break;
          //Second Command.
          //We will not process it here.
          case command_pass_number_end:
            command_state_mc = command_pass_end_of_first_command;
            //printf("STATE_COMMAND_END\n");
            break;
          //We shold not be here.
          default:
            command_state_mc = command_pass_error;
          break;
        }//End switch command_state_mc in command A
        break;
      // Line Command
      case 'L':
        //printf("Command L\n" );
        switch (command_state_mc) {
          //First command
          case command_pass_init:
          case command_pass_start_end_of_path:
            command_state_mc = command_pass_type_found;
            //printf("STATE_COMMAND_FOUND\n");
            strcpy(command+write_index, CMD_MOVE_TYPE_LINE);
            write_index += sizeof(CMD_MOVE_TYPE_LINE) - 1;
            break;
          //Second Command.
          //We will not process it here.
          case command_pass_number_end:
            command_state_mc = command_pass_end_of_first_command;
            //printf("STATE_COMMAND_END\n");
            break;
          //We shold not be here.
          default:
            command_state_mc = command_pass_error;
            break;
          }//End switch command_state_mc in command L
        break;
      //All other text. should be all numbers.
      default:
        //Detected a character that's not part of a command character or white Space
        //printf("NOT a command [%c]\n",file_text_line[read_index]);
        //only numbers are expected here. Check.
        //Please note we do not support negetive number.
        //We work on the first quadrant of the XY plane where all numbers are positive.
        if (file_text_line[read_index] >= '0' && file_text_line[read_index] <= '9') { //(number)
          //printf("Found a number \n");
          switch (command_state_mc) {
            //First number after the command.
            case command_pass_type_found:
              //Add a Space, the command seperator.
              command[write_index]=CMD_COMMAND_SEPERTOR;
              write_index++;
              //Add the numner
              command[write_index]=file_text_line[read_index];
              write_index++;
              //Set the command_state_mc to command_pass_number_found
              command_state_mc = command_pass_number_found;
              //printf("STATE_NUMBER_FOUND\n");
              break;
            //N th digit of a number.
            case command_pass_number_found:
              command[write_index]=file_text_line[read_index];
              write_index++;
              break;
            //N th number of a command.
            case command_pass_number_end:
              //A new number in a sequence, Add a seperator
              command[write_index]=CMD_NUMBER_SEPERATOR;
              write_index++;
              //Add the numner
              command[write_index]=file_text_line[read_index];
              write_index++;
              //Set the command_state_mc to command_pass_number_found
              command_state_mc = command_pass_number_found;
              //printf("STATE_NUMBER_FOUND\n");
            break;
            //Out of sequence.
            case command_pass_start_end_of_path:
            default:
              //printf("Number out of sequence..!\n");
              command_state_mc = command_pass_error;
            break;
          }//end Switch
        }//if (file_text_line[read_index] >= '0' && file_text_line[read_index] <= '9')
        else { //if (file_text_line[read_index] >= '0' && file_text_line[read_index] <= '9')
          //printf("NOT A number \n");
          command_state_mc = command_pass_error;
        }//end if (file_text_line[read_index] >= '0' && file_text_line[read_index] <= '9')
      break;
    }//switch (file_text_line[read_index])

    //We come here after every charactor peoceesing
    //We need to make the decision to exit by looking at the command_state_mc

    switch (command_state_mc) {
      //Error condition.
      case command_pass_error:
        printf("command pass Error\n");
        command[0]='\0'; //clear the buffer.
        //reset the file pointer
        printf("Restore pointer read = %lu Command length = %lu write_index = %d read_index = %d\n", strlen(file_text_line),strlen(command),write_index, read_index);
        fseek(fp,-(strlen(file_text_line)),SEEK_CUR);
        //insted we can return the partial data if needed.
        //calling function SHOULD use the return error stete.
        return(error_bad_command_in_file);
        break;
        //valid delemeter found
      case command_pass_start_end_of_path:
        break;
      //Command read complete at the end of the line
      case command_pass_end_of_command_line:
        printf("command pass complete\n");
        command[write_index] = '\0'; //null terminate
        //reached end of command.
        //please note we do not support commands in multiple lines.
        return(error_success);
        break;
      //Command read completed at the middle of the line.
      case command_pass_end_of_first_command:
        printf("command pass complete in mid of a line\n");
        command[write_index] = '\0'; //null terminate
        //we have to set the file pointer here to the end of the
        //decoded command.
        printf("Restore read pointer read = %lu Command length = %lu write_index = %d read_index = %d\n", strlen(file_text_line),strlen(command),write_index, read_index);
        fseek(fp,-(strlen(file_text_line)-read_index),SEEK_CUR);
        return(error_success);
        break;
      default:
        //No issue, let's go to the next char.
        break;
    }
  }//For loop
  //we should not be here.
  return(error_bad_command_in_file);
}

/*
int find_global_tag
finds a particular tag in the file
The file read happen inside this function, so the file_text_line
pointer moves. Make sure you save the file pointer if you want to
control it in the calling program.
*/

int find_global_tag(FILE *fp, char *tag){

  char file_text_line[GLOBAL_BUFFLEN]; //buffer to read and store file
  int match_offset = 0; //holds the location of last match returned from chec_tag

  do {
     if(NULL != fgets (file_text_line, GLOBAL_BUFFLEN, fp)) {
        //Check for a tag match
        if(error_success==check_tag(file_text_line,tag, &match_offset, string_match_exact_less_leading_space)){

          printf("get_parameter_value: match at position %d of %zu long string '%s'",match_offset,strlen(file_text_line),file_text_line);
          if (file_text_line[match_offset] >= ' '){ //readable char, and not CR/LF
            //We have not reached the end of that line, and there are some text after
            //the match. We have to restore the file pointer to the end of the match.
            if (0 != fseek(fp,-(strlen(file_text_line)-(match_offset)),SEEK_CUR)) {
              printf("Error pointer correction: of string %s to -%zu\n",file_text_line, (strlen(file_text_line)-(match_offset)));
            } //move the file pointer back.
            else{
              printf("Restore File pointer as next char=> ASCII 0x%X CHAR '%c' back -%zu places\n", file_text_line[match_offset], file_text_line[match_offset], (strlen(file_text_line)-(match_offset+1)));
            }
            //Note - the file pointer increments when you .fgets()
          }
          else { //Not a readable char. We have reached to the end of
            printf("Next char is non readable=> ASCII 0x%X CHAR '%c' END\n", file_text_line[match_offset], file_text_line[match_offset]);
          }
          //We come here as there is a match. Let's return.
          return(error_success);
        }//if tag match
        else {//tag does not match
          printf("find_global_tag: match at position %d of %zu long string '%s'",match_offset,strlen(file_text_line),file_text_line);
          //We do not reset the read pointer here, as we will try the next line.
        }
      }
  }while (!feof(fp));
  //We are here as we did not get a match till EOF.
  //We do not reset the file pointer here.
  //The calling function is responsible to restore, when an error occurs.
  return(error_buffer_mismatch);
}


/*
int check_tag(har * buff, char *tag, int *match_offset, int string_match_type)
This function checks if the buffer contain the needed tag (or text)
return 0 if yes.
string_match_type can be any from string_match_type enum. Invalid value will
return error_unknown.
*/

int check_tag(char * buff, char *tag, int *match_offset, int string_match_type) {
  printf("check_tag: '%s' <[FOR]> '%s'\n", buff, tag );
  int index = 0; //pointer to the match try
  int offset = 0; //offset due to extra white space or charactors

  while (buff[index+offset] && tag[index]) { //while either is not NULL, that is end of string
    //printf("match %c %d with %c %d \n",buff[index+offset],buff[index+offset],tag[index],tag[index]);

    if (buff[index+offset] == tag[index]){ //chars match
      //printf("char match at %d\n",index+offset);
      index++;      //next char of both strings
    }
    else {//mismatch
      switch (string_match_type) {//match method changes based in the type
        case string_match_part:
          //Please note the part match work only if there are no mismatch conditions
          //after the match.
          //jusr ignore this mismatch and increment only the buffer pointer
          //printf("looking for part match, so ignore mismatch %c & %c\n",buff[index+offset],tag[index]);
          offset++;
          break;

        case string_match_exact_ignore_leading_mismatch:
          //jusr ignore this mismatch and increment only the buffer pointer
          //printf("Ignore leading mismatch, so ignore mismatch %c & %c\n",buff[index+offset],tag[index]);
          offset++;
          break;

        case string_match_exact_less_leading_space:
          //Check if this is a space and ignore it it yes. we need to increment the buffer pointer.
          //printf("Ignore leading space, so ignore mismatch %c & %c\n",buff[index+offset],tag[index]);
          if (buff[index+offset] == ' ') { //is it white space..?
            //printf("White space at %d\n",index+offset);
            offset++; //white space, next char of buff.
            break;
          }
        //else, the next statement will be run. We do not have a break here.
        //if it's not a space then we have to exit with an arror. We ignore only spaces.
        case string_match_exact:
          printf("miss match at %d\n",index+offset);
          *match_offset = index+offset; //return without incrementing index. we can not use this anyway.
          return(error_buffer_mismatch); //tag mismatch
          break;

        default:
          //we should not be here.
          return(error_unknown);
          break;
      }//Swith
      //we are here due to a mismatch so we need to reset the match index.
      //Please note this prevents part matches in the middle of buffer.
      //This was done intentinally.
      index = 0;
    }//else
  }//while

  if (0==index){ //Not a match
    //printf("Not a match. Scanned %d\n",index+offset);
    *match_offset = index; //This is zero
    return(error_buffer_mismatch); //tag mismatch
  }
  else {//Atleast one charactor matched.
    switch (string_match_type) {
      case string_match_part:
        *match_offset = index+offset; //return without incrementing index. we can not use this anyway.
        return(error_success); //even one char match is a success.
        break;
      case string_match_exact_ignore_leading_mismatch:
      case string_match_exact_less_leading_space:
      case string_match_exact:
        if (index==strlen(tag)) {
          //printf("it is a match\n");
          *match_offset = index+offset;//pointer is already incremented, so points to the next char
          return (error_success); //tag match
        }
        else{
          //printf("it is a match\n");
          *match_offset = index+offset;//pointer is already incremented, so points to the next char
          return (error_buffer_mismatch); //tag match
        }
      default:
          //we should not be here.
          return(error_unknown);
      break;
    }
  }
}
