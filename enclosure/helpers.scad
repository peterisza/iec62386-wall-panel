
module shell(height, th)
{
    linear_extrude(height)
    difference() {
        offset(th) children(); 
        children();             
    }
};

module inner_shell(height, th) {
    linear_extrude(height)
        difference() {
            children();                               
            offset(delta = -th)      
                children();
        }
}

module fucked_up_square(r, w, h)
{
    intersection()
    {
        square([w, h], center=true);
        circle(r = r);
    }
}

module ring(d1, d2, h)
{
    difference() {
        cylinder(d = d1, h = h, center=true);
        cylinder(d = d2, h = h+1, center=true);
    }
}
