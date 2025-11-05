// lekerekített élű lyukak
// chamfered szélű lyukak
// rugalmas támaszték
// support a lyukaknak
// lekerekített tetejű doboz

use <helpers.scad>

$fn = 100;
eps = 0.001;

frame_size = 80;
base_plate_th = 1.2;

base_plate_enforcement_th = 2;
base_plate_enforcement_w = 2;

air_duct_th = 1.2;
air_duct_w = 7;
air_duct_pitch = 19.2;
air_duct_slope = 1.15;

connector_w = 3;
connector_l = 26.6;
connector_dist = 38.6;
connector_r = 1;
connector_h = 9.5;

air_hole_w = 0.1;
air_hole_l = 10;
air_hole_r = 1.5;

standoff_length = 11;
standoff_gap = 0.5;
standoff_th = 0.8;
standoff_w = 5;
standoff_pitch = 31.5;

mounting_hole_dist = 30;
mounting_hole_w = 3.4;
mounting_hole_l = 10;
mounting_hole_base = 0.5;

screw_distance = 17.7;
screw_hole_d = 2.0;
screw_insulation_d = 4.5;
screw_insulation_h = 3.5;
pcb_platform_w = 6;
pcb_th = 1.6;

box_guard_h = 2;
box_guard_th = 2.4;
box_guard_spacing = 0.3;

box_th = 1;
box_size = 46;
box_max_r = 57.6/2;
box_space_behind_pcb = 5;
box_screw_platform_hole_d = 5.2;
box_screw_hole_d = 2.8;
box_screw_tightness = 0.2;
box_panel_spacing = 0.5;
box_screw_head_d = 1.3;

bus_conn_w = 16.5;
bus_conn_h = 8.3;
bus_conn_x = -5.4;

support_th = 0.0;


module air_ducts()
{
    for(a = [0:90:270]) rotate([0,0,a]) {
        mirror([0,0,1]) 
        translate([0, air_duct_pitch, eps])
        linear_extrude(air_duct_th, scale=air_duct_slope)
        square([frame_size+5, air_duct_w], center = true);
    }
}  

module holes_2d()
{
    for(a = [0,180]) rotate([0,0,a]) {
        translate([connector_dist/2, 0, 0])
        offset(connector_r)
        square([connector_w, connector_l], center = true);
        
        translate([0, connector_dist/2, 0])
        offset(air_hole_r)
        square([air_hole_l, air_hole_w], center = true);
    }
}

module holes()
{
    translate([0, 0, -eps])
    linear_extrude(base_plate_th+eps*2)
    holes_2d();
}

module holes_support()
{
    mirror([0,0,1])
    shell(air_duct_th, support_th)
    holes_2d();
}

module standoff_cutout()
{
    for(a = [0:90:270]) rotate([0,0,a]) {
        translate([standoff_pitch, /*-frame_size/2+standoff_length/2+standoff_offset*/standoff_pitch, 0])
        rotate([0,0,45])
        cube([standoff_w, standoff_length, 50], center = true);
    }
}


module standoff()
{
    th = air_duct_th - 0.2;
    for(a = [0:90:270]) rotate([0,0,a]) {
        cutout_length = standoff_length;
        translate([standoff_pitch, standoff_pitch, -standoff_th/2+base_plate_th])
        rotate([0,0,45])
        {
            translate([0, -standoff_gap, 0])
            cube([standoff_w - 2*standoff_gap, standoff_length, standoff_th], center = true);
            
            translate([0, standoff_length/2-standoff_gap*2.5, standoff_th/2 + th/2])
            cube([standoff_w - 2*standoff_gap, 1.5, th], center = true);
        }
    }    
}

module standoff_support()
{
    translate([0,0,+base_plate_th-standoff_th])
    mirror([0,0,1])
    inner_shell(air_duct_th+base_plate_th-standoff_th, support_th)
        projection()
            standoff();
}

module base_plate_enforcement()
{

    for (ang = [45:90:135]) {
        rotate([0, 0, ang])
        translate([0,0,air_duct_th/2+base_plate_enforcement_th/2])
            cube([sqrt(2)*screw_distance*2, base_plate_enforcement_w, base_plate_enforcement_th], center = true);
    }
}

module screw_platform(distance, height, outer, inner)
{
    for (ang = [0:90:270]) {
        rotate([0,0,ang])
        translate([-distance,-distance,-height/2])
        ring(outer, inner, height);
    }
}

module pcb_platform(distance, height, outer, inner)
{
    for (ang = [0:90:270]) {
        rotate([0,0,ang])
        translate([-distance,-distance,-height/2])
        difference() {
            cube([outer, outer, height], center = true);
            cylinder(d = inner, h = height+1, center=true);
        }
    }
}

module frame_pcb_platform()
{
    mirror([0,0,1])
    translate([0, 0, -base_plate_th+eps])
    intersection()
    {
          union() {
            pcb_platform(screw_distance, connector_h, pcb_platform_w, screw_hole_d);
            screw_platform(screw_distance, connector_h+pcb_th+screw_insulation_h, screw_insulation_d, screw_hole_d);
          }
          translate([0, 0, -50])
          linear_extrude(50)
            fucked_up_square(box_max_r-box_th-box_panel_spacing, box_size-box_th*2-1, box_size-box_th*2-1);
    }
}

module box_guard()
{
    spacing = box_guard_spacing;
    translate([0, 0, base_plate_th])
        linear_extrude(box_guard_h)
        intersection() {
            difference() {
                fucked_up_square(box_max_r + spacing + box_guard_th, box_size+spacing*2 + box_guard_th, box_size+spacing*2 + box_guard_th);
                fucked_up_square(box_max_r + spacing, box_size+spacing*2, box_size+spacing*2);
            }
            circle(box_max_r);
        }
}


module screw_head()
{
    cylinder(5, 0.1, 10);
}

module mounting_hole_2d()
{
    offset(r = mounting_hole_w/2)
    intersection() {
        difference() {
            circle(mounting_hole_dist+eps);
            circle(mounting_hole_dist-eps);
        }
        for (ang = [0:90:270])
            rotate([0,0,ang])
            translate([0, mounting_hole_dist, 0])
            circle(mounting_hole_l/2);
    }
}

module mounting_hole()
{
    translate([0,0,base_plate_th - mounting_hole_base])
    mirror([0,0,1]) {
        minkowski() {
            linear_extrude(eps)
                mounting_hole_2d();
            screw_head();
        }
        translate([0, 0, -5])
        linear_extrude(5)
            mounting_hole_2d();
   }
}


module frame()
{
    difference() {
        union() {
            translate([0,0,base_plate_th/2])
            cube([frame_size, frame_size, base_plate_th], center = true);
            
            translate([0,0,-air_duct_th/2])
            cube([frame_size, frame_size, air_duct_th], center = true);
            
            base_plate_enforcement();
            frame_pcb_platform();
            box_guard();
        }
        union() {
            holes();
            air_ducts();
            mounting_hole();
            standoff_cutout();
        }
    }
    standoff();
    holes_support();
    standoff_support();
}

module box()
{
    w = box_size;
    h = box_size;
    
    conn_w = bus_conn_w;
    conn_h = bus_conn_h+box_th;
    conn_x = bus_conn_x;
    conn_d = box_space_behind_pcb + box_th - 0.0;
    
    box_height = connector_h + pcb_th + box_space_behind_pcb + box_th;
    box_screw_platform_h = box_space_behind_pcb - screw_insulation_h - 0.3;
    box_screw_platform_h2 = box_space_behind_pcb + box_screw_tightness;
    
    translate([0, 0, -box_height])
    difference() {
        union() {
            linear_extrude(box_height-0.1)
            difference() {
                fucked_up_square(box_max_r, w, h);
                fucked_up_square(box_max_r-box_th, w-box_th*2, h-box_th*2);
            }
            linear_extrude(box_th)
                fucked_up_square(box_max_r, w, h);
            intersection() {
                union()
                {
                    translate([0,0,box_screw_platform_h2+box_th])
                        pcb_platform(screw_distance, box_screw_platform_h2, 7.7, box_screw_platform_hole_d);
                    translate([0,0,box_screw_platform_h+box_th])
                        screw_platform(screw_distance, box_screw_platform_h, 7.7, box_screw_hole_d);
                }
                linear_extrude(box_height-0.1)
                    fucked_up_square(box_max_r-box_th, w-box_th*2, h-box_th*2);
            }
           /* translate([conn_x, +h/2-conn_h/2-box_th/2+0.01, -0.01+conn_d/2])
                cube([conn_w+box_th*2, conn_h+box_th, conn_d], center=true);*/
        }
        union() {
            translate([conn_x, +h/2-conn_h/2+0.01, -0.01+conn_d/2])
                cube([conn_w, conn_h, conn_d], center=true);
            translate([0,0,box_th+eps])
                screw_platform(screw_distance, 15, box_screw_hole_d, 0);            
            /*translate([0,0,1])
                screw_platform(screw_distance, 1, 6, box_screw_hole_d);*/
            for (ang = [0:90:270]) {
                rotate([0, 0, ang])
                translate([screw_distance, screw_distance, box_screw_head_d])
                mirror([0,0,1])
                screw_head();
            }
        }
    }
    
                /*for (ang = [0:90:270])
                rotate([0, 0, ang])
                translate([screw_distance, screw_distance, -box_height+box_screw_head_d])
                mirror([0,0,1])
                screw_head();*/
}



//translate([0, 0, -50]) box();
frame();
