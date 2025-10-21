size = 80.5;            
plate_thickness = 2.4;       
air_duct_th = 1.2;
air_duct_w = 8;
screw_enforcement_thickness = 1;
screw_head_height = 2;
pcb_th = 1.6;
box_size = 46;
box_max_r = 57.6/2;
conn_d = 10.5;
behind_pcb = 5;
screw_insulation_h = 3.5;
box_th = 1;
box_corner_r = 6;
clip_width = 10;
clip_h = 2;
screw_distance = 17.7;
screw_hole_d = 3.0;
panel_screw_insulation_d = 5.4;
box_screw_platform_hole_d = 5.9;
box_screw_hole_d = 3.6;
box_screw_tightness = 0.2;
box_panel_spacing = 0.2;
centered = true;
center = true;
box_start_z = plate_thickness/2;
box_height = conn_d + pcb_th + behind_pcb + box_th;
frame_th = 0;
frame_h = 0;

$fn=100;


module ring(d1, d2, h)
{
    difference() {
        cylinder(d = d1, h = h, center=true);
        cylinder(d = d2, h = h+1, center=true);
    }
}

module screw_head_holes(w, l, h) {
    distance = 30;        // a furatok közepe a középponttól (mm)
    hole_d = 15;
    
    for (ang = [0:90:270]) {
        rotate([0,0,ang])
        translate([distance, 0, 0])
        linear_extrude(h, scale=2.3, center=true)
        projection(cut = true)
        translate([-distance, 0, 0])
        intersection() {
                translate([distance, 0, 0])
                cylinder(d = l, h = h, center=true);

                ring(distance*2+w, distance*2-w, h);
        }
    };
}

module screw_holes(w, l, h) {
    distance = 30;        // a furatok közepe a középponttól (mm)
    hole_d = 15;
    
    intersection() {
        for (ang = [0:90:270]) {
            rotate([0,0,ang])
            translate([distance, 0, 0])
            cylinder(d = l, h = h, center=true);
        };
        ring(distance*2+w, distance*2-w, h);
    }
}



module connector_holes()
{
    distance = 38.6;
    margin = 0.25;
    translate([-distance/2, 0, 0])
       cube([2.65+margin, 25.6+margin, 20], center = true);
    translate([distance/2, 0, 0])
       cube([2.65+margin, 25.6+margin, 20], center = true);
    translate([0, -distance/2, 0])
        cube([14+margin, 2.65+margin, 20], center = true);
    translate([0, distance/2, 0])
        cube([14+margin, 2.65+margin, 20], center = true);
}

module main_plate()
{
    difference() {
        translate([0, 0, +frame_h/2])
        cube([size+2*frame_th, size+2*frame_th, plate_thickness+frame_h], center = centered);
        translate([0, 0, plate_thickness/2+frame_h/2+0.001])
        cube([size, size, frame_h], center = centered);
    }
}

module air_ducts()
{
    w = air_duct_w;
    t = air_duct_th+frame_h;
    pitch = 18;
    displacement = plate_thickness/2-t/2+0.001+frame_h;
    
    translate([-pitch, 0, displacement])
        cube([w, size+5, t], center = true);
    translate([pitch, 0, displacement])
        cube([w, size+5, t], center = true);
    translate([0, pitch, displacement])
        cube([size+5, w, t], center = true);
    translate([0, -pitch, displacement])
        cube([size+5, w, t], center = true);
}

module screw_platform(height, outer, inner)
{
    distance = screw_distance;
    for (ang = [0:90:270]) {
        rotate([0,0,ang])
            translate([-distance,-distance,-height/2])
                ring(outer, inner, height);
    }
}

module pcb_platform(height, outer, inner)
{
    distance = screw_distance;
    for (ang = [0:90:270]) {
        rotate([0,0,ang])
            translate([-distance,-distance,-height/2])
                difference() {
                    cube([outer, outer, height], center = true);
                    cylinder(d = inner, h = height+1, center=true);
                }
    }
}

module rounded_square(r, w, h)
{
    offset(r=r)
        offset(delta=-r)
            square([w, h], center=true);
}

module fucked_up_square(r, w, h)
{
        intersection()
        {
            square([w, h], center=true);
            circle(r = r);
        }
}

module box_guard()
{
    size = 2;
    h = clip_h + 1.5;
    spacing = 0.3;
    guard_h = 2;
    translate([0, 0, -box_start_z-guard_h])
        linear_extrude(guard_h)
        intersection() {
            difference() {
                fucked_up_square(box_max_r + spacing + 2.4, box_size+spacing*2+2.4, box_size+spacing*2+2.4);
                fucked_up_square(box_max_r + spacing, box_size+spacing*2, box_size+spacing*2);
            }
            /*fucked_up_square(box_max_r, box_size, box_size);*/
            circle(box_max_r);
        }
}

module box()
{
    w = box_size;
    h = box_size;
    r = box_corner_r;
    
    conn_w = 16.5;
    conn_h = 8.2+box_th;
    conn_x = -5.2;
    conn_d = behind_pcb + box_th - 0.0;
    
    box_screw_platform_h = behind_pcb - screw_insulation_h - 0.3;
    box_screw_platform_h2 = behind_pcb + box_screw_tightness;
    
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
                        pcb_platform(box_screw_platform_h2, 8.2, box_screw_platform_hole_d);
                    translate([0,0,box_screw_platform_h+box_th])
                        screw_platform(box_screw_platform_h, 7.7, box_screw_hole_d);
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
            translate([0,0,box_th])
                screw_platform(15, box_screw_hole_d, 0);            
            translate([0,0,1])
                screw_platform(1, 6, box_screw_hole_d);
        }
    }
}



/*translate([0, 0, -box_start_z])
box();*/


difference() {
  union() {
   main_plate();
      
   translate([0, 0, -box_start_z])
   intersection()
    {
          union() {
            pcb_platform(conn_d, 8, screw_hole_d);
            screw_platform(conn_d+pcb_th+screw_insulation_h, panel_screw_insulation_d, screw_hole_d);
          }
          translate([0, 0, -50])
          linear_extrude(50)
            fucked_up_square(box_max_r-box_th-box_panel_spacing, box_size-box_th*2-1, box_size-box_th*2-1);
    }
    box_guard();
  }
  union() {
    screw_holes(4, 10, 10);
      translate([0, 0, -screw_head_height/2+plate_thickness/2+0.01])
    screw_head_holes(4, 10, screw_head_height);
    connector_holes();
    air_ducts();
  }
}
