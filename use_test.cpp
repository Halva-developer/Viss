#line 1 "use_test.viss"
use IOstream for *
#line 2 "use_test.viss"
use math for sin, PI
#line 3 "use_test.viss"
use net for *

#line 5 "use_test.viss"
int main() {
#line 6 "use_test.viss"
    Dec s = sin(PI / 2);
#line 7 "use_test.viss"
    viss::io::println("Sin is: " + viss::toStr(s));
    
#line 9 "use_test.viss"
    using main {
#line 10 "use_test.viss"
        return 0;
#line 11 "use_test.viss"
    }
#line 12 "use_test.viss"
}
