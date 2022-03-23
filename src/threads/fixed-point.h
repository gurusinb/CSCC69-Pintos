#define FRACTIONAL_BITS 16
#define FACTOR (1<<FRACTIONAL_BITS) 

#define add_fixed_point(x,y) ((int)(x+y))
#define subtract_fixed_point(x,y) ((int)(x-y))
#define multiply_fixed_point(x,y) ((int)(((int64_t)x)*y/FACTOR)) 
#define divide_fixed_point(x,y) ((int)(((int64_t)x)*FACTOR/y)) 

#define int_to_fixed_point(x) ((int)(x*FACTOR))
#define fixed_point_to_int(x) ((int)(x/FACTOR))
#define nearest_int(x) (x>=0) ? ((int)((x+FACTOR/2)/FACTOR)) : ((int)((x-FACTOR/2)/FACTOR))  

#define add_int_fixed_point(x,y) ((int)(x+(y*FACTOR)))
#define subtract_int_fixed_point(x,y) ((int)(x-(y*FACTOR)))
#define multiply_int_fixed_point(x,y) ((int)(x*y))
#define divide_int_fixed_point(x,y) ((int)(x/y))