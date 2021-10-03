#include <stdio.h>
#include <math.h>
#include <path_gen.h>
#include <cnc_2d_common.h>

#define GLOBAL_SAFE_HEIGHT 10.0 //the height that the head is free to move around
#define GLOBAL_SAFE_SPEED 80.0  //Move the tool below surface height, but where there is no material
#define GLOBAL_SURFASE_HEIGHT 0.0 //Furface level (Z)
#define GLOBAL_XY_FEED_RATE 50.0 //Cutting feed z_feed_rate
#define GLOBAL_Z_FEED_RATE 20.0 //Cutting feed Z axis
#define GLOBAL_FIRST_PASS 1     //1 for cuts that does not start from the tool position. The first move will happen in safe height.
#define GLOBAL_NOT_FIRST_PASS 0 //0 for cuts that does start from the tool position. This is for repeat passes
#define GLOBAL_TAB_COUNT 9  //No tabs
#define GLOBAL_TAB_LENGTH 1.5 //Tab lenth
#define GLOBAL_DEFAULT_TOOL_OFFSET 0.0 //run in the center line.
#define GLOBAL_DEFAULT_TOOL_WIDTH 2.0 //run in the center line.

enum g_move_types {
  g_rapid_travel=0,
  g_linear_interpolation=1,
  g_circular_interpolation_clockwise=2,
  g_circular_interpolation_counterclockwise=3,
  g_dwell=4
}; /* this matches the gcodes, G0, G1..etc*/

typedef struct g_coordinate {
  float x;
  float y;
  float z;
} g_coordinate;

/* Properties of a cut.
*/
typedef struct g_cut_properties {
  float cut_depth;      //final depth level (lower the deeper)
  float surface_level;  //Serface level
  float safe_height;    //Safe hieght for G0 moves
  float xy_feed_rate;   //cut speed on X Y
  float z_feed_rate;    //rate for the tool cut in Z
  float safe_feed_rate; //This is to move the tool below surface, where there is no material
  float tool_width;     //Width of the milling tool
  float tool_offset;    //offset from center line
} g_cut_properties;

/*  Start point and end point are obvious parameters for a move.
    circular ineterpoltion has one extra param, that's where the
    center of the circle is. Here we keep all of them in absolue
    coordinates.
*/
typedef struct g_move_properties {
  enum g_move_types move_type;  //move types supported by gcodes
  g_coordinate abs_from;        //move from
  g_coordinate abs_to;          //move to
  g_coordinate abs_arc_center;  //this is the only extra parameter we need for now.
  float arc_radius;
  float arc_angle_rad;
  float move_speed; //feed rate.
} g_move_properties;

int cnc_cut (g_move_properties move_prop, g_cut_properties cut_prop, g_coordinate *tool_location, int tab_count, float tab_length, char *gcode_buffer, int *write_count);

int cut_circle (float radius, g_coordinate center, g_coordinate *tool_location, g_cut_properties cut_prop, int tab_count, float tab_length, char *gcode_buffer, int *write_count);
int move_tool_to_z(g_move_properties move, char *gcode_buffer, int *write_count);
int move_tool_to_xy(g_move_properties move, char *gcode_buffer, int *write_count);

g_coordinate transform_abs_to_relative(g_coordinate transform, g_coordinate reletive_to);

int main(int argc, char *argv[])  {

        char gcode_buffer[GLOBAL_BUFFLEN] = "Gcode Buffer";
        FILE * fp;

        int gcode_buffer_offset = 0; //location if the next character to be saved.
        int gcode_buffer_write_count = 0; //place holder for the crite count
//        int *p_gcode_buffer_write_count = &gcode_buffer_write_count; //pointer to buff_write_count.

//        g_coordinate start_point = {0.0, 0.0, GLOBAL_SAFE_HEIGHT}; //not used
//        g_coordinate datum = {0.0,0.0,0.0}; //not used
        g_coordinate tool_location = {0.0, 0.0, GLOBAL_SAFE_HEIGHT}; //global tool location.
        //pointer to above will be passed to all tool move functions.
        //this will change outside the main function.

        /*
        argument processor
        */

        for (int arg_index = 0; arg_index < argc; arg_index++)
         printf("main(): argument# %d %s\n",arg_index, argv[arg_index] );

        if (argc < 2) {
          printf("main(); Usage error. Insufficient argumemts.\n");
          return(error_error_insufficient_argumemtsother);
        }
       //First Argument is the input cmn and the next is the output gcode file.

       //First check if the arg[2] is valid.

        fp = fopen(argv[2], "w+");

        fputs("(28mm Circle Inner);\n", fp);
        fputs("G90; (Absolute coordinates)\n", fp);
        fputs("G94; (Feed rate units per minute)\n", fp);
        fputs("G91.1; (arc distnce is reletive)\n", fp);//GBRL does not support 90.1
        fputs("G40; (tool rad comp cancel)\n", fp);
        fputs("G49; (tool length comp cancel)\n", fp);
        fputs("G17; (XY cord)\n", fp);
        fputs("G21; (mm unit)\n", fp);
        fputs("G55; (coordinate system to center 0, 0)\n", fp);
        fputs("F50;\n", fp);
        fputs("S1000;\n", fp);
        fputs("M3;\n", fp);
        fputs("G0 Z10;\n", fp); //Safe SafeHeight
        fputs("G0 X0 Y0;\n", fp); //Go to Datum.
        fputs("(Calculations Start here)\n", fp); //Go to Datum.

        //Cut a hole;
        g_cut_properties cut_prop = {GLOBAL_SAFE_HEIGHT,GLOBAL_SAFE_HEIGHT,GLOBAL_SAFE_HEIGHT,GLOBAL_XY_FEED_RATE,GLOBAL_Z_FEED_RATE,GLOBAL_SAFE_SPEED, GLOBAL_DEFAULT_TOOL_WIDTH, GLOBAL_DEFAULT_TOOL_OFFSET}; //{Cut depth, Surface, Safe, XY feed, Z Feed, safe feed, FirstPass}
        g_coordinate circle_centre = {0.0,0.0,0.0};
        float radius = 20;//in mm
        float depth = 3.8; // in mm
        float depth_pitch = 0.5; //in mm

        //Set the Cut props here
        cut_prop.cut_depth = 0.0; //Depth of first cut. One cut at surace level for safety
        cut_prop.surface_level = 0.0; //Keep 1mm above for safety.
        cut_prop.xy_feed_rate = 100; //go in a differnet speed than the default.
        cut_prop.tool_width = 2.0;
        cut_prop.tool_offset = -1.0; // 1.0mm to left

        printf("main() Hole Size = %4.2fmm, Hole depth = %4.2fmm, Tool Width = %4.2fmm\r\n", radius, depth, cut_prop.tool_width);

        //Make sure the gcode_buffer is big enough for the function to copy the Gcodes.
        //There is no error checking here.

        printf("main() Loop till Cut Depth = %4.2f < %4.2f\n",cut_prop.cut_depth, (-depth));

        for(;cut_prop.cut_depth>(-depth); cut_prop.cut_depth-=depth_pitch) {
          printf("main() Cut at = %4.2f\r\n", cut_prop.cut_depth);
          if (error_success != cut_circle(radius, circle_centre, &tool_location, cut_prop, GLOBAL_TAB_COUNT, GLOBAL_TAB_LENGTH, gcode_buffer, &gcode_buffer_write_count)) {
              printf("!ERROR\n");
              return(1);
          };
          if (gcode_buffer_write_count) fputs(gcode_buffer,fp);  //Save to file
          //cut_prop.is_first_pass=GLOBAL_NOT_FIRST_PASS; // Second pass

        } /* for loop */

        //Last cut
        cut_prop.cut_depth=-depth;
        cut_prop.xy_feed_rate=50; //Slow down.
        printf("main() Final Cut at = %4.2f\r\n", cut_prop.cut_depth);

        if (error_success != cut_circle(radius, circle_centre, &tool_location, cut_prop, GLOBAL_TAB_COUNT, GLOBAL_TAB_LENGTH, gcode_buffer, &gcode_buffer_write_count)) {
            printf("!ERROR\n");
            return(1);
        };
        if (gcode_buffer_write_count) fputs(gcode_buffer,fp);  //Save to file

        fputs("G0 Z10; (lift tool)\n", fp);
        fputs("G0 X0 Y0;\n", fp);
        fputs("M5;\n", fp);

        fclose(fp);
        return (0);
}/* main() */


int cnc_cut (g_move_properties move_prop, g_cut_properties cut_prop, g_coordinate *tool_location, int tab_count, float tab_length, char *gcode_buffer, int *write_count) {
  return(0);
} /*cnc_cut*/


/*
Function: g_coordinate cut_circle ()
Description:  Generates Gcode for perfect circle with safety tabs to keep it in place till the last cut.
              This will generate code for one round.
Parameters:
float radius  radius of the circle
g_coordinate center absolue coordinates of the center
*g_tool_location pass the current tool location.
g_cut_properties  cut_prop holds the propertes of the cut.
                  cut_prop.is_first_pass = 1, the tool will make few extra moves.
                  recomend moving the tool to the circle center before this.
float tool_width  tool width in mm
int tab_count     how many safety tabs to keep
float tab_length  tab length in mm
char *gcode_buffer  the buffer that hold the gcode strings.
*/

int cut_circle (float radius, g_coordinate center, g_coordinate *g_tool_location, g_cut_properties cut_prop, int tab_count, float tab_length, char *gcode_buffer, int *write_count) {

  char *p_buff;           //pointer to start copy location in Buffer.
  p_buff = gcode_buffer;  //Set the pointer to start of the Buffer.
  *p_buff = '\0';         //clear the buffer.
  *write_count = 0;       //set the count to zero.

  int buff_offset = 0;      //location if the next character to be saved.
  int buff_write_count = 0; //place holder for the write count
  //int *p_buff_write_count = &buff_write_count; //pointer to buff_write_count.

  int number_of_arcs = tab_count ? tab_count :  1 ; //if tab_count = 0, set the arcs to 1
  tab_length = tab_count ? tab_length : 0.0; //if tab count is zero, set tab length to zero

  printf("cut_circle() Hole Size = %4.2fmm, %d Tabs of %4.2fmm , Arcs = %d, Tool Width = %4.2fmm\r\n", radius, tab_count,tab_length, number_of_arcs, cut_prop.tool_width);
  printf("cut_circle() Center x = %4.2f, Center y = %4.2f \r\n", center.x, center.y);
  printf("cut_circle() Cut depth = %4.2fmm, SafeHeight = %4.2fmm Cut Speed = %4.2fmm/min\r\n", cut_prop.cut_depth, cut_prop.safe_height, cut_prop.xy_feed_rate);

  g_coordinate tool_location = *g_tool_location;
  g_coordinate tool_move_to_location = tool_location; //initialize to same location

  printf("cut_circle() Tool locaton X = %4.2f, Y = %4.2f, Z = %4.2f\r\n", tool_location.x, tool_location.y, tool_location.z);

  //do the calculations here

  //Set the toolpath offset
  float tool_plath_radius=radius + cut_prop.tool_offset;
  //float tab_arc_angle = (360*tab_length)/(2*M_PI*tool_plath_radius);
  float tab_arc_angle_rad = tab_length/tool_plath_radius;
  //float cut_arc_angle = (360/number_of_arcs) - tab_arc_angle;
  float cut_arc_angle_rad = (2 * M_PI / number_of_arcs) - tab_arc_angle_rad;

  //printf("cut_circle() Tool Path Radius = %4.2fmm, Tab Arc Angle = %4.2fdeg Cut Arc Angle = %4.2fdeg\r\n", tool_plath_radius, tab_arc_angle, cut_arc_angle);
  printf("cut_circle() Tool Path Radius = %4.2fmm, Tab Arc Angle = %4.2frad Cut Arc Angle = %4.2frad\r\n", tool_plath_radius, tab_arc_angle_rad, cut_arc_angle_rad);

  g_move_properties tool_move = {g_rapid_travel,tool_location, tool_move_to_location,center,cut_prop.safe_feed_rate}; //tool move propertes

  //Generate gcode
  if (1) {//cut_prop.is_first_pass) {

    tool_location = center; //assume the tool is at the circle center
    tool_move_to_location = tool_location; //initialize to same location

    //go to safe height
    tool_move_to_location.z = cut_prop.safe_height; //set the height.
    tool_move.move_type = g_rapid_travel;           //rapid travel
    tool_move.abs_to = tool_move_to_location;       //to locaiton
    tool_move.abs_from = tool_location;             //from location
    tool_move.move_speed = cut_prop.safe_feed_rate; //speed (this is N/A for rapid)

    if (error_success != move_tool_to_z(tool_move,p_buff, &buff_write_count)){
      printf("! Error %d\r\n", __LINE__ );
      return(error_in_move);
    }
    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

    //go to the left most edge of the Circle
    tool_move_to_location.x = tool_location.x - tool_plath_radius;
    tool_move.abs_to = tool_move_to_location;       //to locaiton
    tool_move.abs_from = tool_location;             //from location
    if (error_success != move_tool_to_xy(tool_move,p_buff, &buff_write_count)){
      printf("! Error %d\r\n", __LINE__ );
      return(error_in_move);
    }
    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

    //Z Fast down to Surface + 1. remember, Z+ is away from the work piece
    tool_move_to_location.z = cut_prop.surface_level + 1; //set the height.
    tool_move.move_type = g_linear_interpolation;         //let's go safe speed
    tool_move.abs_to = tool_move_to_location;       //to locaiton
    tool_move.abs_from = tool_location;             //from location
    tool_move.move_speed = cut_prop.safe_feed_rate; //speed (this is N/A for rapid)

    if (error_success != move_tool_to_z(tool_move,p_buff, &buff_write_count)){
      printf("! Error %d\r\n", __LINE__ );
      return(error_in_move);
    }
    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

  }/*if (cut_prop.is_first_pass)*/

  //Cut

  float running_angle_rad = 0; //we store the active angle in this variable.
  tool_move.abs_arc_center = center;

  for (int arc_number = 1; arc_number <= number_of_arcs; arc_number ++) {

    running_angle_rad = running_angle_rad + cut_arc_angle_rad;
    printf("cut_circle() CUT : go to Angle = %4.2f, Arc number = %d \n",running_angle_rad,arc_number);

    //Z down to cut level
    tool_move_to_location.z = cut_prop.cut_depth;   //go down to cut level
    tool_move.move_type = g_linear_interpolation;   //not needed, as this is already set.
    tool_move.abs_to = tool_move_to_location;      //to locaiton
    tool_move.abs_from = tool_location;            //from location
    tool_move.move_speed = cut_prop.z_feed_rate;   //speed, go slow. we are near the work piece

    if (error_success != move_tool_to_z(tool_move,p_buff, &buff_write_count)){
      printf("! Error %d\r\n", __LINE__ );
      return(error_in_move);
    }
    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

    //Cut @ xy feed rate

    tool_move_to_location.x = -tool_plath_radius * cos(running_angle_rad);
    tool_move_to_location.y =  tool_plath_radius * sin(running_angle_rad);
    tool_move.move_type = g_circular_interpolation_clockwise;
    tool_move.abs_to = tool_move_to_location;      //to locaiton
    tool_move.abs_from = tool_location;            //from location
    tool_move.move_speed = cut_prop.xy_feed_rate;   //speed, go slow. we are near the work piece

    if (error_success != move_tool_to_xy(tool_move,p_buff, &buff_write_count)){
      printf("! Error %d\r\n", __LINE__ );
      return(error_in_move);
    }
    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

    if(tab_arc_angle_rad) { //if we have to leave tabs

      running_angle_rad = running_angle_rad + tab_arc_angle_rad; //tab_arc_angle_rad <> 0
      printf("cut_circle() TAB : go to Angle = %4.2f, Arc number = %d \n",running_angle_rad,arc_number);
      //lift tool above surface
      tool_move_to_location.z = cut_prop.surface_level + 1; //set the height.
      tool_move.move_type = g_linear_interpolation;         //let's go safe speed
      tool_move.abs_to = tool_move_to_location;       //to locaiton
      tool_move.abs_from = tool_location;             //from location
      tool_move.move_speed = cut_prop.safe_feed_rate; //speed (this is N/A for rapid)

      if (error_success != move_tool_to_z(tool_move,p_buff, &buff_write_count)){
        printf("! Error %d\r\n", __LINE__ );
        return(error_in_move);
      }
      tool_location = tool_move_to_location; //successful move so update the tool location here.
      printf("written %d, buff = %s\n", buff_write_count, p_buff);
      p_buff += buff_write_count;       //move the buffer pointer
      *write_count += buff_write_count; //this is anyway updated at the end.

      //Move @ xy feed rate

      tool_move_to_location.x = -tool_plath_radius * cos(running_angle_rad);
      tool_move_to_location.y = tool_plath_radius * sin(running_angle_rad);
      tool_move.move_type = g_circular_interpolation_clockwise;
      tool_move.abs_to = tool_move_to_location;      //to locaiton
      tool_move.abs_from = tool_location;            //from location
      tool_move.move_speed = cut_prop.xy_feed_rate;   //speed, go slow. we are near the work piece

      if (error_success != move_tool_to_xy(tool_move,p_buff, &buff_write_count)){
        printf("! Error %d\r\n", __LINE__ );
        return(error_in_move);
      }
    }/*if(tab_arc_angle_rad)*/

    tool_location = tool_move_to_location; //successful move so update the tool location here.
    printf("written %d, buff = %s\n", buff_write_count, p_buff);
    p_buff += buff_write_count;       //move the buffer pointer
    *write_count += buff_write_count; //this is anyway updated at the end.

  } /*(int arc_number = 1; arc_number <= number_of_arcs; arc_number ++)*/

  printf("calculated write_count= %d\r\n", *write_count);
  *write_count = p_buff - gcode_buffer;
  printf("actual write_count= %d, gcode_buffer =\r\n%s\r\n", *write_count, gcode_buffer);
  //update the tool locaton
  *g_tool_location = tool_location;
  printf("cut_circle() Tool locaton X = %4.2f, Y = %4.2f, Z = %4.2f\r\n", tool_location.x, tool_location.y, tool_location.z);

  return(error_success); //We are supposed to return the tool location. Need to implement it.
} /* cut_circle() */

int move_tool_to_z(g_move_properties move, char *gcode_buffer, int *write_count) {

    *gcode_buffer = '\0';   //clean the buffer
    *write_count = 0;       //set the write count to zero.

    switch (move.move_type) {
      case g_rapid_travel:
        printf("move_tool_to_z() G00 Z%4.2f\r\n",move.abs_to.z);
        *write_count = sprintf(gcode_buffer,"G00 Z%4.2f;\r\n",move.abs_to.z);
        return (error_success);
      case g_linear_interpolation:
        printf("move_tool_to_z ()G01 Z%4.2f\r\n",move.abs_to.z);
        *write_count = sprintf(gcode_buffer,"G01 Z%4.2f F%4.2f;\r\n",move.abs_to.z, move.move_speed);
        return (error_success);
      case g_circular_interpolation_clockwise:
        printf("ERROR! move_tool_to_z() move not implemented\r\n");
        *write_count = sprintf(gcode_buffer,"! Not implemented\r\n");//This will Generate an error in the output buffer
        return (error_function_not_implemented);
      case g_circular_interpolation_counterclockwise:
        printf("ERROR! move_tool_to_z() move not implemented\r\n");
        *write_count = sprintf(gcode_buffer,"! Not implemented\r\n");//This will Generate an error in the output buffer
        return (error_function_not_implemented);
      case g_dwell:
        printf("ERROR! move_tool_to_z() move not implemented\r\n");
        *write_count = sprintf(gcode_buffer,"! Not implemented\r\n");//This will Generate an error in the output buffer
        return (error_function_not_implemented);
      default:
        printf("ERROR! move_tool_to_z() Bad parameter\r\n");
        *write_count = sprintf(gcode_buffer,"! Bad parameter\r\n");//This will Generate an error in the output buffer
        return (error_other);
        break; //we will not be here.
    }
    //we should not be here.
    return (error_unknown);
}


int move_tool_to_xy(g_move_properties move, char *gcode_buffer, int *write_count) {

  printf("move_tool_to_xy() We are now at: X=%4.2f Y=%4.2f Z=%4.2f\r\n", move.abs_from.x, move.abs_from.y, move.abs_from.z);

  switch (move.move_type) {
    case g_rapid_travel:
      printf("move_tool_to_xy() G00 X%4.2f Y%4.2f\r\n",move.abs_to.x, move.abs_to.y);
      *write_count = sprintf(gcode_buffer,"G00 X%4.2f Y%4.2f;\r\n",move.abs_to.x, move.abs_to.y);
      return (error_success);
    case g_linear_interpolation:
      printf("move_tool_to_xy() G01 X%4.2f Y%4.2f\r\n",move.abs_to.x, move.abs_to.y);
      *write_count = sprintf(gcode_buffer,"G01 X%4.2f Y%4.2f F%4.2f;\r\n",move.abs_to.x, move.abs_to.y, move.move_speed);
      return (error_success);
    case g_circular_interpolation_clockwise:
      printf("move_tool_to_xy() G02 X%4.2f Y%4.2f I%4.2f J%4.2f\n",move.abs_to.x, move.abs_to.y, transform_abs_to_relative(move.abs_arc_center,move.abs_from).x,transform_abs_to_relative(move.abs_arc_center,move.abs_from).y);
      *write_count = sprintf(gcode_buffer,"G02 X%4.2f Y%4.2f I%4.2f J%4.2f F%4.2f\n",move.abs_to.x, move.abs_to.y, transform_abs_to_relative(move.abs_arc_center,move.abs_from).x,transform_abs_to_relative(move.abs_arc_center,move.abs_from).y, move.move_speed);
      return (error_success);
    case g_circular_interpolation_counterclockwise:
      printf("ERROR! move_tool_to_xy() move not implemented\r\n");
      *write_count = sprintf(gcode_buffer,"! Not implemented\r\n");//This will Generate an error in the output buffer
      return (error_function_not_implemented);
    case g_dwell:
      printf("ERROR! move_tool_to_xy() move not implemented\r\n");
      *write_count = sprintf(gcode_buffer,"! Not implemented\r\n");//This will Generate an error in the output buffer
      return (error_function_not_implemented);
    default:
      printf("ERROR! move_tool_to_z() Bad parameter\r\n");
      *write_count = sprintf(gcode_buffer,"! Bad parameter\r\n");//This will Generate an error in the output buffer
      return (error_other);
      break; //we will not be here.
  } /*move.move_type*/

  return(error_unknown);
}

g_coordinate transform_abs_to_relative(g_coordinate transform, g_coordinate reletive_to) {

  transform.x = transform.x - reletive_to.x;
  transform.y = transform.y - reletive_to.y;
  transform.z = transform.z - reletive_to.z;

  return (transform);
}
