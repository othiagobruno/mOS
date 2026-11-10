/* gcc -O1 -o /tmp/mapkit_host_test tools/mapkit_host_test.c userland/libsystem/mapkit_geom.c -lm */
#include <stdio.h>
#include <math.h>
#include <string.h>
typedef struct { double latitude, longitude; } CLLocationCoordinate2D;
typedef struct { double latitudeDelta, longitudeDelta; } MKCoordinateSpan;
typedef struct { CLLocationCoordinate2D center; MKCoordinateSpan span; } MKCoordinateRegion;
typedef struct { double x, y; } MKMapPoint;
typedef struct { double width, height; } MKMapSize;
typedef struct { MKMapPoint origin; MKMapSize size; } MKMapRect;
extern const MKMapRect MKMapRectWorld, MKMapRectNull;
MKMapPoint MKMapPointForCoordinate(CLLocationCoordinate2D);
CLLocationCoordinate2D MKCoordinateForMapPoint(MKMapPoint);
MKCoordinateRegion MKCoordinateRegionForMapRect(MKMapRect);
MKCoordinateRegion MKCoordinateRegionMakeWithDistance(CLLocationCoordinate2D, double, double);
double MKMetersPerMapPointAtLatitude(double);
double MKMapPointsPerMeterAtLatitude(double);
double MKMetersBetweenMapPoints(MKMapPoint, MKMapPoint);
int MKMapRectContainsRect(MKMapRect, MKMapRect);
MKMapRect MKMapRectInset(MKMapRect, double, double);
MKMapRect MKMapRectUnion(MKMapRect, MKMapRect);
MKMapRect MKMapRectIntersection(MKMapRect, MKMapRect);
typedef struct { char domain[32]; int code; } mk_fail_t;
void mk_search_fail(mk_fail_t *);
void mk_directions_fail(mk_fail_t *);
static int fail;
static void expect(int ok, const char *m) { if (!ok) { fprintf(stderr, "FALHOU %s\n", m); fail++; } }
int main(void) {
    CLLocationCoordinate2D z = {0, 0};
    MKMapPoint p = MKMapPointForCoordinate(z);
    CLLocationCoordinate2D back;
    MKMapRect a, b, u, ix, ins;
    MKCoordinateRegion reg;
    mk_fail_t e, d;
    expect(fabs(p.x - MKMapRectWorld.size.width / 2) < 1, "longitude 0 no meio");
    expect(fabs(p.y - MKMapRectWorld.size.height / 2) < 1, "latitude 0 no meio");
    back = MKCoordinateForMapPoint(p);
    expect(fabs(back.latitude) < 1e-6 && fabs(back.longitude) < 1e-6, "volta ao 0,0");
    p = MKMapPointForCoordinate((CLLocationCoordinate2D){37.7749, -122.4194});
    back = MKCoordinateForMapPoint(p);
    expect(fabs(back.latitude - 37.7749) < 1e-4 && fabs(back.longitude + 122.4194) < 1e-4, "sao francisco");
    reg = MKCoordinateRegionMakeWithDistance(z, 1000, 1000);
    expect(reg.span.latitudeDelta > 0 && fabs(reg.span.latitudeDelta - reg.span.longitudeDelta) < 1e-9, "metros no equador");
    a.origin.x = 0; a.origin.y = 0; a.size.width = 100; a.size.height = 80;
    reg = MKCoordinateRegionForMapRect(a);
    expect(reg.span.latitudeDelta > 0 && reg.span.longitudeDelta > 0, "regiao do retangulo");
    b.origin.x = 50; b.origin.y = 20; b.size.width = 10; b.size.height = 10;
    expect(MKMapRectContainsRect(a, b) && !MKMapRectContainsRect(b, a), "contem");
    expect(!MKMapRectContainsRect(MKMapRectNull, a), "nulo nao contem");
    u = MKMapRectUnion(a, MKMapRectNull);
    expect(u.size.width == 100, "uniao com nulo");
    ix = MKMapRectIntersection(a, b);
    expect(ix.size.width == 10 && ix.origin.x == 50, "intersecao");
    b.origin.x = 200;
    expect(isinf(MKMapRectIntersection(a, b).origin.x), "sem intersecao");
    ins = MKMapRectInset(a, 5, 5);
    expect(ins.origin.x == 5 && ins.size.width == 90, "inset");
    expect(fabs(MKMetersPerMapPointAtLatitude(0) * MKMapPointsPerMeterAtLatitude(0) - 1) < 1e-9, "metro por ponto");
    expect(MKMetersBetweenMapPoints(a.origin, b.origin) > 0, "distancia");
    mk_search_fail(&e);
    mk_directions_fail(&d);
    expect(strcmp(e.domain, "MKErrorDomain") == 0 && e.code == 2, "busca sem rede");
    expect(strcmp(d.domain, "MKErrorDomain") == 0 && d.code == 5, "rota sem rede");
    if (fail) { fprintf(stderr, "%d falhas\n", fail); return 1; }
    printf("PASSOU\n");
    return 0;
}
