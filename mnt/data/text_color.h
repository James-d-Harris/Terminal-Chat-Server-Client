//
//  text_color.h
//
//  Created by Wolf-Dieter Otte on 3/24/23.
//

#ifndef text_color_h
#define text_color_h

#define RESET_COLOR "\e[0m"

#define JOINED_COLOR "\e[30m\e[42m\e[1m"
#define LEFT_COLOR   "\e[30m\e[41m\e[1m"
#define NOTE_COLOR   "\e[30m\e[107m\e[1m"

/*
 
General Pattern
   "\e[XXm" or
   "\033[XXm"

             Foregrnd    Backgrnd
Default      \033[39m    \033[49m
 
Black        \033[30m    \033[40m
Dark red     \033[31m    \033[41m
Dark green   \033[32m    \033[42m
Dark yellow  \033[33m    \033[43m
Dark blue    \033[34m    \033[44m
Dark magenta \033[35m    \033[45m
Dark cyan    \033[36m    \033[46m
Light gray   \033[37m    \033[47m
 
Dark gray    \033[90m    \033[100m
Red          \033[91m    \033[101m
Green        \033[92m    \033[102m
Orange       \033[93m    \033[103m
Blue         \033[94m    \033[104m
Magenta      \033[95m    \033[105m
Cyan         \033[96m    \033[106m
White        \033[97m    \033[107m
 
Special Controls
reset = 0
bold  = 1

*/

#endif /* text_color_h */

