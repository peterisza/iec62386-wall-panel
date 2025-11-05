//translate([121, 0, 0])

/*difference() {
    cube([100, 100, 1], center = true);
    cube([80, 80, 2], center = true);
}*/


r = 80/897;

th = 0.5;
translate([0,0,-th/2])
cube([80,80,th], center=true);


intersection()
{
    mirror([1, 0, 0])
    scale([r, r, r*10])
    translate([0, 0, 100])
    surface(file="button1.png", center=true, convexity=1, invert = true);
    
    cube([101, 101, 2*2], center=true);
}


/*    
difference() {
    cube([103, 103, 1.6], center = true);
    cube([101, 101, 4], center = true);
}
*/
