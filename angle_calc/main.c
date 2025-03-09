#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "help.h"

#define REAR_OFFSET     21
#define LOWER_LEG_LEN   40
#define UPPER_LEG_LEN   24
#define PI              3.14159

typedef struct {
    int id;
    int value;
    char letter;
} fun_data_t;

void calc_leg_angles(int x, int y, int* angle1, int* angle2) {
    double front_angle, rear_angle;
    double a1, a2, b1, b2;     // angles
    double hypo1, hypo2;

    // Calc hypotenuse assuming 0, 0 is on the front servo axis
    hypo1 = sqrt(pow(x, 2) + pow(y, 2));
    hypo2 = sqrt(pow(x - REAR_OFFSET, 2) + pow(y, 2));

    a1 = acos(x / hypo1);
    a2 = acos((REAR_OFFSET - x)/hypo2);
    b1 = acos((pow(UPPER_LEG_LEN, 2) - pow(LOWER_LEG_LEN, 2) - pow(hypo1, 2))/(-2*LOWER_LEG_LEN*hypo1));
    b2 = acos((pow(UPPER_LEG_LEN, 2) - pow(LOWER_LEG_LEN, 2) - pow(hypo2, 2))/(-2*LOWER_LEG_LEN*hypo2));

    *angle1 = (180*(a1 + b1) / PI) - 135;
    *angle2 = 135 - (180*(a2 + b2) / PI);
}

int main(int argc, char* argv[]) {
    int x = 10;
    int y = 15;
    
    if(argc == 3) {
        x = atoi(argv[1]);
        y = atoi(argv[2]);
    }

    int theta1, theta2;

    calc_leg_angles(x ,y, &theta1, &theta2);
    
    printf("theta1 = %d, theta2 = %d\n", theta1, theta2);
    
    fun_data_t fun_data;
    fun_data.id = 6;
    fun_data.value = 55;
    fun_data.letter = 'a';

    printf("I think this should be an addy: %i\n", fun_data);
    
    return 0;
}
