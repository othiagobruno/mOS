/* Geometria do MapKit, sem Objective-C. Projeção de Mercator da Apple
 * (o mundo cabe em 2^28 pontos). Sem rede: busca e rota só descrevem a falha. */
typedef struct { double latitude, longitude; } CLLocationCoordinate2D;
typedef struct { double latitudeDelta, longitudeDelta; } MKCoordinateSpan;
typedef struct { CLLocationCoordinate2D center; MKCoordinateSpan span; } MKCoordinateRegion;
typedef struct { double x, y; } MKMapPoint;
typedef struct { double width, height; } MKMapSize;
typedef struct { MKMapPoint origin; MKMapSize size; } MKMapRect;

double log(double), tan(double), cos(double), sqrt(double);

enum { MK_ERR_SERVER = 2, MK_ERR_DIRECTIONS = 5 };
static const double WORLD = 268435456.0;
static const double LAT_LIM = 85.0511287798066;
static const double PI = 3.14159265358979323846;
static const double EARTH = 6378137.0;

const MKMapRect MKMapRectWorld = {{0, 0}, {268435456.0, 268435456.0}};
const MKMapRect MKMapRectNull = {{__builtin_huge_val(), __builtin_huge_val()}, {0, 0}};

static int is_inf(double x) { return x != 0 && x == 2 * x; }
static int rect_null(MKMapRect r) { return is_inf(r.origin.x) || is_inf(r.origin.y); }
static double clamp_lat(double lat) {
    if (lat > LAT_LIM) return LAT_LIM;
    if (lat < -LAT_LIM) return -LAT_LIM;
    return lat;
}
static double dabs(double x) { return x < 0 ? -x : x; }

static double exp_s(double x) {
    double t = 1, s = 1;
    int i;
    if (x > 20) x = 20;
    if (x < -20) x = -20;
    for (i = 1; i < 30; i++) { t *= x / i; s += t; }
    return s;
}
static double atan_s(double z) {
    int neg = z < 0, i;
    double a, t, s;
    if (neg) z = -z;
    if (z > 1) {
        a = PI / 2 - atan_s(1 / z);
        return neg ? -a : a;
    }
    t = z; s = z;
    for (i = 1; i < 40; i++) { t *= -z * z; s += t / (2 * i + 1); }
    return neg ? -s : s;
}
static double atan_sinh(double n) {
    double e = exp_s(n);
    double sinh = (e - 1.0 / e) / 2.0;
    return atan_s(sinh);
}

MKMapPoint MKMapPointForCoordinate(CLLocationCoordinate2D c) {
    double lat = clamp_lat(c.latitude) * PI / 180.0;
    MKMapPoint p;
    p.x = (c.longitude + 180.0) / 360.0 * WORLD;
    p.y = (1.0 - log(tan(lat) + 1.0 / cos(lat)) / PI) / 2.0 * WORLD;
    return p;
}
CLLocationCoordinate2D MKCoordinateForMapPoint(MKMapPoint p) {
    double n = PI - 2.0 * PI * p.y / WORLD;
    CLLocationCoordinate2D c;
    c.longitude = p.x / WORLD * 360.0 - 180.0;
    c.latitude = 180.0 / PI * atan_sinh(n);
    return c;
}
double MKMetersPerMapPointAtLatitude(double latitude) {
    return cos(clamp_lat(latitude) * PI / 180.0) * 2.0 * PI * EARTH / WORLD;
}
double MKMapPointsPerMeterAtLatitude(double latitude) {
    double m = MKMetersPerMapPointAtLatitude(latitude);
    return m == 0 ? 0 : 1.0 / m;
}
double MKMetersBetweenMapPoints(MKMapPoint a, MKMapPoint b) {
    CLLocationCoordinate2D ca = MKCoordinateForMapPoint(a), cb = MKCoordinateForMapPoint(b);
    double mx = MKMetersPerMapPointAtLatitude((ca.latitude + cb.latitude) / 2.0);
    double dx = a.x - b.x, dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy) * mx;
}
MKCoordinateRegion MKCoordinateRegionMakeWithDistance(CLLocationCoordinate2D center, double latMeters, double lonMeters) {
    MKCoordinateRegion r;
    double latRad = clamp_lat(center.latitude) * PI / 180.0;
    r.center = center;
    r.span.latitudeDelta = (latMeters / EARTH) * (180.0 / PI);
    r.span.longitudeDelta = (lonMeters / (EARTH * cos(latRad))) * (180.0 / PI);
    return r;
}
MKCoordinateRegion MKCoordinateRegionForMapRect(MKMapRect rect) {
    MKMapPoint mid, b;
    CLLocationCoordinate2D ca, cb;
    MKCoordinateRegion r;
    r.center.latitude = r.center.longitude = 0;
    r.span.latitudeDelta = r.span.longitudeDelta = 0;
    if (rect_null(rect) || rect.size.width < 0 || rect.size.height < 0) return r;
    mid.x = rect.origin.x + rect.size.width / 2;
    mid.y = rect.origin.y + rect.size.height / 2;
    b.x = rect.origin.x + rect.size.width;
    b.y = rect.origin.y + rect.size.height;
    r.center = MKCoordinateForMapPoint(mid);
    ca = MKCoordinateForMapPoint(rect.origin);
    cb = MKCoordinateForMapPoint(b);
    r.span.latitudeDelta = dabs(ca.latitude - cb.latitude);
    r.span.longitudeDelta = dabs(ca.longitude - cb.longitude);
    return r;
}
int MKMapRectContainsRect(MKMapRect a, MKMapRect b) {
    if (rect_null(a) || rect_null(b)) return 0;
    return b.origin.x >= a.origin.x && b.origin.y >= a.origin.y
        && b.origin.x + b.size.width <= a.origin.x + a.size.width
        && b.origin.y + b.size.height <= a.origin.y + a.size.height;
}
MKMapRect MKMapRectInset(MKMapRect r, double dx, double dy) {
    if (rect_null(r)) return MKMapRectNull;
    r.origin.x += dx; r.origin.y += dy;
    r.size.width -= 2 * dx; r.size.height -= 2 * dy;
    return r;
}
MKMapRect MKMapRectUnion(MKMapRect a, MKMapRect b) {
    double x0, y0, x1, y1;
    MKMapRect u;
    if (rect_null(a)) return b;
    if (rect_null(b)) return a;
    x0 = a.origin.x < b.origin.x ? a.origin.x : b.origin.x;
    y0 = a.origin.y < b.origin.y ? a.origin.y : b.origin.y;
    x1 = a.origin.x + a.size.width;
    if (b.origin.x + b.size.width > x1) x1 = b.origin.x + b.size.width;
    y1 = a.origin.y + a.size.height;
    if (b.origin.y + b.size.height > y1) y1 = b.origin.y + b.size.height;
    u.origin.x = x0; u.origin.y = y0; u.size.width = x1 - x0; u.size.height = y1 - y0;
    return u;
}
MKMapRect MKMapRectIntersection(MKMapRect a, MKMapRect b) {
    double x0, y0, x1, y1;
    MKMapRect u;
    if (rect_null(a) || rect_null(b)) return MKMapRectNull;
    x0 = a.origin.x > b.origin.x ? a.origin.x : b.origin.x;
    y0 = a.origin.y > b.origin.y ? a.origin.y : b.origin.y;
    x1 = a.origin.x + a.size.width;
    if (b.origin.x + b.size.width < x1) x1 = b.origin.x + b.size.width;
    y1 = a.origin.y + a.size.height;
    if (b.origin.y + b.size.height < y1) y1 = b.origin.y + b.size.height;
    if (x1 < x0 || y1 < y0) return MKMapRectNull;
    u.origin.x = x0; u.origin.y = y0; u.size.width = x1 - x0; u.size.height = y1 - y0;
    return u;
}

typedef struct { char domain[32]; int code; } mk_fail_t;
static void putdom(mk_fail_t *e, int code) {
    const char *s = "MKErrorDomain";
    int i;
    e->code = code;
    for (i = 0; s[i] && i < 31; i++) e->domain[i] = s[i];
    e->domain[i] = 0;
}
void mk_search_fail(mk_fail_t *e) { if (e) putdom(e, MK_ERR_SERVER); }
void mk_directions_fail(mk_fail_t *e) { if (e) putdom(e, MK_ERR_DIRECTIONS); }
