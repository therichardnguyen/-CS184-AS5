#include "main.h"

using namespace std;

//****************************************************
// Global Variables
//****************************************************
enum {AMBIENT, DIFFUSE, SPECULAR, ALL};
World * world;
Film * film;
int frameCount = 0, lightsOn = ALL;
bool rightClick = false, blinnphong = false;
double rightClickX = 0, rightClickY = 0, ksm = 0, ksp = 0;

// use this to multiply colors:
inline vec3 pairwiseMult(const vec3 &a, const vec3 &b) {
	return vec3(a[0]*b[0], a[1]*b[1], a[2]*b[2]);
}

// kd(CI) max(^I * ^n, 0)
inline vec3 diffuse(double kd, vec3 color, vec3 light,vec3 i, vec3 n ) {
	return kd*pairwiseMult(color,light)*max(i.normalize() * n.normalize(), 0.0);
}

// S = (Ksm) C + (1 - ksm) (1,1,1)
inline vec3 specularHighlight(double ksm, vec3 color) {
	return ksm*color + (1-ksm)*vec3(1,1,1);
}

// ks(SI) max(r * v, 0)ksp
inline vec3 specular(double ks, double ksm, double ksp, vec3 color, vec3 light, vec3 r, vec3 v) {
	return ks * pairwiseMult(specularHighlight(ksm,color),light) * pow(max(r.normalize()*v.normalize(),0.0),ksp);
}

//-------------------------------------------------------------------------------
// Here you raycast a single ray, calculating the color of this ray.

vec3 raycast(Ray & ray) {
	vec3 retColor(0,0,0);
	vec3 d = -vec3(ray.direction().normalize(),VW);
	
    // storage for raycast
	double t; vec3 n; MaterialInfo m; vec3 i; vec3 s; vec3 r; vec3 v; 
	// storage for shadowray
	Ray shadow; double ts; vec3 ns; MaterialInfo ms; double falloff;

	if (world->intersect(ray, t, n, m)) {
		vec4 pos = ray.getPos(t);
		// ambient light
		if (lightsOn == AMBIENT || lightsOn == ALL)
			retColor += pairwiseMult(m.color, world->getAmbientLight()) * m.k[MAT_KA];
		
		for(vector<Light>::iterator it = world->getLightsBeginIterator(LIGHT_DIRECTIONAL); 
			it != world->getLightsEndIterator(LIGHT_DIRECTIONAL); it++) {
			
			i = (-1*(it->getDirection()));
			
			vec4 pos(ray.getPos(t));
			vec4 dir(i,0);
			vec4 src(pos+dir);
			Ray shadow(pos,src,0.00001);
			double ts; vec3 ns; MaterialInfo ms;
			
			// diffuse light
			if(!(world->intersect(shadow,ts,ns,ms))){
				if (lightsOn == DIFFUSE || lightsOn == ALL) 
					retColor += diffuse(m.k[MAT_KD], m.color, it->getLightInfo().color, i, n);
		
				//specular light
				if (lightsOn == SPECULAR || lightsOn == ALL) {
					if (!blinnphong) {
			 			r = ((-1*i) +2*(i*n)*n);
						retColor += specular(m.k[MAT_KS], m.k[MAT_KSM], m.k[MAT_KSP], m.color, it->getLightInfo().color, r, d);
					} else {
						r = (d + i.normalize());
						retColor += specular(m.k[MAT_KS], m.k[MAT_KSM], m.k[MAT_KSP], m.color, it->getLightInfo().color, r, n);
					}
				}
			}
		}
	
		for(vector<Light>::iterator it = world->getLightsBeginIterator(LIGHT_POINT); 
			it != world->getLightsEndIterator(LIGHT_POINT); it++) {
			
			i = (it->getPosition() - vec3(ray.getPos(t),VW));
			falloff = pow((1/(abs(i.length()) + it->getLightInfo().deadDistance)),it->getLightInfo().falloff);
			
			vec4 intersection(ray.getPos(t));
			vec4 src(it->getPosition(),1);
			vec4 dir = intersection-src;
			vec4 hit = intersection + dir*.00001;
			Ray shadow(hit,src,-0.00001);
			
			double ts; vec3 ns; MaterialInfo ms;
			// diffuse light
			if(!(world->intersect(shadow,ts,ns,ms) && ts < 1)){
				if (lightsOn == DIFFUSE || lightsOn == ALL) 
					retColor += diffuse(m.k[MAT_KD], m.color, falloff*it->getLightInfo().color, i, n);
		
				// specular light
				if (lightsOn == SPECULAR || lightsOn == ALL) {
					if(!blinnphong) {	
						r = ((-1*i) + 2*(i*n)*n);
						retColor += specular(m.k[MAT_KS], m.k[MAT_KSM], m.k[MAT_KSP], m.color, falloff*it->getLightInfo().color, r, d);
					} else {
						r = (d + i.normalize());
						retColor += specular(m.k[MAT_KS], m.k[MAT_KSM], m.k[MAT_KSP], m.color, falloff*it->getLightInfo().color, r, n);
					}
				}
			}
		}
	 }

	// clip the colors if they're too intense
	if (retColor[0] > 1.0)
		retColor[0] = 1.0;
	if (retColor[1] > 1.0) 
		retColor[1] = 1.0;
	if (retColor[2] > 1.0)
		retColor[2] = 1.0;

	return retColor;
}
//-------------------------------------------------------------------------------
/// The display function
void display() {

	//Clear Buffers
    glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);					// indicate we are specifying camera transformations
	glLoadIdentity();							// make sure transformation is "zero'd"

	// Calls the raycast method on each pixel
	// sampled using the Viewport and Ray classes.
	// and stores the result using the Film class
	film->clear();
	Viewport &view = world->getViewport();
    view.resetSampler();
	vec2 point; Ray ray;
    while(view.getSample(point, ray)) {
		ray.transform(view.getViewToWorld());
        vec3 c = raycast(ray);
        film->expose(c, point);
    }
	film->show();

	//Now that we've drawn on the buffer, swap the drawing buffer and the displaying buffer.
	glutSwapBuffers();
}


//-------------------------------------------------------------------------------
/// \brief	Called when the screen gets resized.
/// This gives you the opportunity to set up all the relevant transforms.
///
void reshape(int w, int h) {
	//Set up the viewport to ignore any size change.
	Viewport &view = world->getViewport();
	glViewport(0,0,view.getW(),view.getH());

	//Set up the PROJECTION transformationstack to be a simple orthographic [-1, +1] projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, view.getW(), 0, view.getH(), 1, -1);

	//Set the MODELVIEW transformation stack to the identity.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}


//-------------------------------------------------------------------------------
/// Called to handle keyboard events.
void myKeyboardFunc(unsigned char key, int x, int y) {
	switch (key) {
        case 's':
            film->saveFrame("./","raycaster"); // image saving has been moved to the film class
            break;
		case 'b': 			// switch from Phong to Blinn-Phong model for shading model
			blinnphong = !blinnphong;
			glutPostRedisplay();
			break;
		case 9: 			// tab to switch from ambient,diffuse,specular,all
			lightsOn = (lightsOn + 1) % 4;
			glutPostRedisplay();
			break;
		case 27:			// Escape key
			exit(0);
			break;
	}
}

void myMouseFunc(int button, int state, int x, int y) {
	if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) 
		cout << " x " << x << " y " << y << endl;
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		cout << "right click down " << endl;
		rightClick = true;
		rightClickX = x;
		rightClickY = y;
		ksm = world->getSphere(0)->getMaterial().k[MAT_KSM];
		ksp = world->getSphere(0)->getMaterial().k[MAT_KSP];
	} 
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
		cout << "right click up " << endl;		
		rightClick = false;
		world->getSphere(0)->getMaterial().k[MAT_KSM] = ksm;
		world->getSphere(0)->getMaterial().k[MAT_KSP] = ksp;
		glutPostRedisplay();
	}
} 

void myMotionFunc(int x, int y) {
	double boundedX = min(max(x,0), IMAGE_WIDTH);
	double boundedY = min(max(abs(y-IMAGE_HEIGHT),2), IMAGE_HEIGHT);
    if (rightClick) {
		world->getSphere(0)->setK(MAT_KSM, ((double) boundedX) / ((double) IMAGE_WIDTH));
		world->getSphere(0)->setK(MAT_KSP, (boundedY / ((double) IMAGE_HEIGHT))*256);
		glutPostRedisplay();
	}
}




//-------------------------------------------------------------------------------
/// Initialize the environment
int main(int argc,char** argv) {
	
	//Initialize OpenGL
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);

	// Load the world
	world = new World(argv[1]);
	// Allocate film for rendering
	film = new Film(IMAGE_WIDTH, IMAGE_HEIGHT);

	//Create OpenGL Window
	glutInitWindowSize(IMAGE_WIDTH,IMAGE_HEIGHT);
	glutInitWindowPosition(0,0);
	glutCreateWindow("CS184 Raycaster - Richard Nguyen");
	//Register event handlers with OpenGL.
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(myKeyboardFunc);
	glutMouseFunc(myMouseFunc);
	glutMotionFunc(myMotionFunc);
	glutPassiveMotionFunc(myMotionFunc);

	//And Go!
	glutMainLoop();
}
