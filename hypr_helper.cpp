#include <gtkmm.h>
#include <cairomm/context.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <mutex>
#include <gtk-layer-shell.h>

// ===== CLOCK =====
class ClockWidget : public Gtk::DrawingArea {
public:
    ClockWidget(){ set_size_request(100,100); }

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        auto a=get_allocation();
        double cx=a.get_width()/2.0, cy=a.get_height()/2.0;
        double r=std::min(a.get_width(),a.get_height())/2.6;

        time_t now=time(0);
        tm *lt=localtime(&now);

        double sec=lt->tm_sec*M_PI/30;
        double min=lt->tm_min*M_PI/30;
        double hr=(lt->tm_hour%12)*M_PI/6;

        cr->set_source_rgba(1,1,1,0.1);
        cr->arc(cx,cy,r,0,2*M_PI);
        cr->stroke();

        cr->set_source_rgb(1,1,1);
        cr->move_to(cx,cy);
        cr->line_to(cx+r*0.5*sin(hr),cy-r*0.5*cos(hr));
        cr->stroke();

        cr->move_to(cx,cy);
        cr->line_to(cx+r*0.7*sin(min),cy-r*0.7*cos(min));
        cr->stroke();

        cr->set_source_rgb(1,0,0.4);
        cr->move_to(cx,cy);
        cr->line_to(cx+r*0.9*sin(sec),cy-r*0.9*cos(sec));
        cr->stroke();

        return true;
    }
};

// ===== METER =====
class CircleMeter : public Gtk::DrawingArea {
public:
    double value=0, display=0;
    std::string label="";

    CircleMeter(){ set_size_request(95,95); }

    void set_value(double v){ value=v; queue_draw(); }

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        auto a=get_allocation();
        double cx=a.get_width()/2.0, cy=a.get_height()/2.0;
        double r=std::min(a.get_width(),a.get_height())/2.6;

        display += (value-display)*0.1;

        cr->set_line_width(3);
        cr->set_source_rgba(1,1,1,0.15);
        cr->arc(cx,cy,r,0,2*M_PI);
        cr->stroke();

        cr->set_line_width(6);
        cr->set_source_rgba(1,0,0.4,0.7);
        cr->arc(cx,cy,r,-M_PI/2,(-M_PI/2)+(2*M_PI*(display/100)));
        cr->stroke();

        cr->set_source_rgb(1,1,1);
        cr->set_font_size(14);

        std::string t=std::to_string((int)display)+"%";
        Cairo::TextExtents ext;
        cr->get_text_extents(t,ext);
        cr->move_to(cx-ext.width/2,cy);
        cr->show_text(t);

        cr->set_font_size(10);
        cr->get_text_extents(label,ext);
        cr->move_to(cx-ext.width/2,cy+16);
        cr->show_text(label);

        return true;
    }
};

// ===== MAIN =====
class HyprHelper : public Gtk::Window {
public:
    HyprHelper(){

        set_title("Hypr Helper");
        set_default_size(280,340);
        set_decorated(false);
        set_keep_above(true);

        gtk_layer_init_for_window(GTK_WINDOW(this->gobj()));
        gtk_layer_set_layer(GTK_WINDOW(this->gobj()), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(GTK_WINDOW(this->gobj()), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(this->gobj()), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

        // CSS (user-safe)
        auto css = Gtk::CssProvider::create();
        try {
            std::string path = Glib::get_home_dir() + "/hypr_helper/style.css";
            css->load_from_path(path);
        } catch(...) {}

        Gtk::StyleContext::add_provider_for_screen(
            Gdk::Screen::get_default(),
            css,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        cpu_meter.label="CPU";
        ram_meter.label="RAM";
        amd_meter.label="AMD";
        nvidia_meter.label="NVIDIA";

        grid.set_row_spacing(6);
        grid.set_column_spacing(6);

        auto center = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        center->pack_start(grid, Gtk::PACK_SHRINK);
        add(*center);

        // CALENDAR
        auto cal = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        cal->get_style_context()->add_class("card");
        calendar_label.set_use_markup(true);
        cal->pack_start(calendar_label);
        grid.attach(*cal,0,0,1,1);

        // CLOCK
        auto clk = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
        clk->get_style_context()->add_class("card");
        clk->pack_start(clock);
        clk->pack_start(time_label);
        clk->pack_start(date_label);
        grid.attach(*clk,1,0,1,1);

        // METERS
        auto mbox = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
        mbox->get_style_context()->add_class("card");
        mbox->pack_start(cpu_meter);
        mbox->pack_start(ram_meter);
        mbox->pack_start(amd_meter);
        mbox->pack_start(nvidia_meter);
        grid.attach(*mbox,0,1,2,1);

        // CLIMATE
        climate.set_xalign(0.5);
        grid.attach(climate,0,2,2,1);

        // BUTTONS
        auto btn = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
        btn->get_style_context()->add_class("card");

        auto reload = Gtk::make_managed<Gtk::Button>("Reload");
        auto monitor = Gtk::make_managed<Gtk::Button>("Monitor");
        auto exit_btn = Gtk::make_managed<Gtk::Button>("Exit");

        reload->signal_clicked().connect([]{ system("hyprctl reload &"); });
        monitor->signal_clicked().connect([]{
            system("command -v btop && kitty -e btop || kitty -e htop &");
        });
        exit_btn->signal_clicked().connect([this]{ this->close(); });

        btn->pack_start(*reload,true,true);
        btn->pack_start(*monitor,true,true);
        btn->pack_start(*exit_btn,true,true);

        grid.attach(*btn,0,3,2,1);

        Glib::signal_timeout().connect(sigc::mem_fun(*this,&HyprHelper::update),1000);

        show_all_children();
    }

private:
    Gtk::Grid grid;
    Gtk::Label calendar_label,time_label,date_label,climate;
    CircleMeter cpu_meter,ram_meter,amd_meter,nvidia_meter;
    ClockWidget clock;

    // 🔥 CALENDAR FUNCTION
    std::string calendar(){
    time_t now=time(0);
    tm *lt=localtime(&now);

    int year=lt->tm_year+1900;
    int month=lt->tm_mon;
    int today=lt->tm_mday;

    tm first={};
    first.tm_year=lt->tm_year;
    first.tm_mon=month;
    first.tm_mday=1;
    mktime(&first);

    int start=first.tm_wday;

    int days=31;
    if(month==1) days=((year%4==0&&year%100!=0)||year%400==0)?29:28;
    else if(month==3||month==5||month==8||month==10) days=30;

    const char* m[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

    std::stringstream ss;

    // 🔥 TITLE
    ss<<"<span font='11' weight='bold'>" << m[month] << " " << year << "</span>\n";

    // 🔥 WEEK HEADER
    ss<<"<span foreground='#888'>Su Mo Tu We Th Fr Sa</span>\n";

    int pos=0;
    for(int i=0;i<start;i++){ ss<<"   "; pos++; }

    for(int d=1; d<=days; d++){
        if(d==today){
            // 🔥 RED HIGHLIGHT (LIKE YOUR ORIGINAL)
            ss<<"<span foreground='#ffffff' background='#ff003c' weight='bold'>";
            if(d<10) ss<<" ";
            ss<<d<<"</span>";
        } else {
            if(d<10) ss<<" ";
            ss<<d;
        }

        ss<<" ";
        pos++;
        if(pos%7==0) ss<<"\n";
    }

    return ss.str();
}

    int cpu_temp(){
        std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
        int t=0; if(f.is_open()) f>>t;
        return t/1000;
    }

    int amd_gpu(){
        for(int i=0;i<5;i++){
            std::string path = "/sys/class/drm/card" + std::to_string(i) + "/device/gpu_busy_percent";
            std::ifstream f(path);
            if(f.is_open()){
                int val; f>>val;
                return val;
            }
        }
        return 0;
    }

    double cpu(){
        std::ifstream f("/proc/stat"); std::string l; getline(f,l);
        std::istringstream ss(l); std::string c;
        long u,n,s,i; ss>>c>>u>>n>>s>>i;
        static long lt=0,li=0;
        long t=u+n+s+i;
        double dt=t-lt,di=i-li;
        lt=t; li=i;
        return dt?(1.0-di/dt)*100:0;
    }

    double ram(){
        std::ifstream f("/proc/meminfo");
        std::string k; long total=0,avail=0;
        while(f>>k){
            if(k=="MemTotal:") f>>total;
            else if(k=="MemAvailable:") f>>avail;
            else f.ignore(256,'\n');
        }
        return (1.0 - (double)avail/total)*100;
    }

    bool update(){
        // 🔥 FIXED
        calendar_label.set_markup(calendar());

        cpu_meter.set_value(cpu());
        ram_meter.set_value(ram());
        amd_meter.set_value(amd_gpu());

        int temp = cpu_temp();

        std::string state;
        if(temp > 80) state="🔥 HOT ";
        else if(temp > 60) state="🌿 NORMAL ";
        else state="❄️ COOL ";

        climate.set_text(state + std::to_string(temp) + "°C");

        return true;
    }
};

int main(int argc,char* argv[]){
    auto app = Gtk::Application::create(argc, argv, "hypr.helper");
    HyprHelper w;
    return app->run(w);
}
