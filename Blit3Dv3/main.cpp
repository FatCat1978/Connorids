/*
Example program that demonstrates collision handling
*/
#include "Blit3D.h"

#include <random>
#include "Camera.h"
#include "Physics.h"
#include "Entity.h"
#include "PaddleEntity.h"
#include "BallEntity.h"
#include "BrickEntity.h"
#include "GroundEntity.h"
#include "Particle.h"

#include "MyContactListener.h" //for handling collisions


void LoadMap(std::string fileName);
Blit3D *blit3D = NULL;

//this code sets up memory leak detection
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#define DEBUG_NEW new(_CLIENT_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif


//GLOBAL DATA
std::mt19937 rng;
std::uniform_real_distribution<float> plusMinus5Degrees(-5, +5);
std::vector<Particle*> particleList;
b2Vec2 gravity; //defines our gravity vector
b2World *world; //our physics engine

				// Prepare for simulation. Typically we use a time step of 1/60 of a
				// second (60Hz) and ~10 iterations. This provides a high quality simulation
				// in most game scenarios.
int32 velocityIterations = 8;
int32 positionIterations = 3;
float timeStep = 1.f / 60.f; //one 60th of a second
float elapsedTime = 0; //used for calculating time passed

std::vector<Entity *> entityList; //most of the entities in our game go here

//contact listener to handle collisions between important objects
MyContactListener *contactListener;

float cursorX = 0;
PaddleEntity *paddleEntity = NULL;

enum GameState { START, PLAYING };
GameState gameState = START;
bool attachedBall = true; //is the ball ready to be launched from the paddle?
int lives = 3;
int score = 0;

float powerCooldown = 5.f;

std::vector<Entity *> ballEntityList; //track the balls seperately from everything else

std::vector<Sprite*> debrisList;

//Sprites 
Sprite *logo = NULL;
Sprite *ballSprite = NULL;
Sprite *redBrickSprite = NULL;
Sprite *yellowBrickSprite = NULL;
Sprite *orangeBrickSprite = NULL;
Sprite *paddleSprite = NULL;
Camera2D* camera = NULL;
//font
AngelcodeFont *debugFont = NULL;

void Init()
{
	debrisList.push_back(blit3D->MakeSprite(0, 57, 64, 55, "Media\\spritesheet_debris.png"));
	debrisList.push_back(blit3D->MakeSprite(0, 168, 64, 52, "Media\\spritesheet_debris.png"));
	debrisList.push_back(blit3D->MakeSprite(0, 446, 68, 51, "Media\\spritesheet_debris.png"));
	//make a camera
	camera = new Camera2D();

	//set it's valid pan area
	camera->minX = -blit3D->screenWidth / 2;
	camera->minY = -blit3D->screenHeight /2;
	camera->maxX = blit3D->screenWidth / 2;
	camera->maxY = blit3D->screenHeight / 2;

	camera->PanTo(blit3D->screenWidth / 2, blit3D->screenHeight / 2);

	//seed random generator
	std::random_device rd;
	rng.seed(rd());

	//turn off the cursor
	blit3D->ShowCursor(false);

	debugFont = blit3D->MakeAngelcodeFontFromBinary32("Media\\DebugFont_24.bin");

	redBrickSprite = blit3D->MakeSprite(0, 0, 64, 32, "Media\\red.png");
	orangeBrickSprite = blit3D->MakeSprite(0, 32, 64, 32, "Media\\orange.png");
	yellowBrickSprite = blit3D->MakeSprite(0, 64, 64, 32, "Media\\yellow.png");

	logo = blit3D->MakeSprite(0, 0, 600, 309, "Media\\connoroid.png");
	ballSprite = blit3D->MakeSprite(0, 0, 32, 32, "Media\\ball.png");
	paddleSprite = blit3D->MakeSprite(0, 0, 128, 32, "Media\\paddle.png");;

	//from here on, we are setting up the Box2D physics world model

	// Define the gravity vector.
	gravity.x = 0.f;
	gravity.y = 0.f;

	// Construct a world object, which will hold and simulate the rigid bodies.
	world = new b2World(gravity);
	//world->SetGravity(gravity); <-can call this to change gravity at any time
	world->SetAllowSleeping(true); //set true to allow the physics engine to 'sleep" objects that stop moving

								   //_________GROUND OBJECT_____________
								   //make an entity for the ground
	GroundEntity *groundEntity = new GroundEntity();
	//A bodyDef for the ground
	b2BodyDef groundBodyDef;
	// Define the ground body.
	groundBodyDef.position.Set(0, 0);

	//add the userdata
	groundBodyDef.userData.pointer = reinterpret_cast<uintptr_t>(groundEntity);

	// Call the body factory which allocates memory for the ground body
	// from a pool and creates the ground box shape (also from a pool).
	// The body is also added to the world.
	groundEntity->body = world->CreateBody(&groundBodyDef);

	//an EdgeShape object, for the ground
	b2EdgeShape groundBox;

	// Define the ground as 1 edge shape at the bottom of the screen.
	b2FixtureDef boxShapeDef;

	boxShapeDef.shape = &groundBox;

	//bottom
	groundBox.SetTwoSided(b2Vec2(0, 0), b2Vec2(blit3D->screenWidth / PTM_RATIO, 0));

	//Create the fixture
	groundEntity->body->CreateFixture(&boxShapeDef);
	
	//add to the entity list
	entityList.push_back(groundEntity);

	//remove the entity pointer from the body definition, so that the other edges
	//are not categorized in a way that causes teh player to lose a life
	//when the ball hits those edges
	groundBodyDef.userData.pointer = NULL;

	//now make the other 3 edges of the screen on a seperate body
	b2Body *screenEdgeBody = world->CreateBody(&groundBodyDef);

	//left
	groundBox.SetTwoSided(b2Vec2(0, blit3D->screenHeight / PTM_RATIO), b2Vec2(0, 0));
	screenEdgeBody->CreateFixture(&boxShapeDef);

	//top
	groundBox.SetTwoSided(b2Vec2(0, blit3D->screenHeight / PTM_RATIO),
		b2Vec2(blit3D->screenWidth / PTM_RATIO, blit3D->screenHeight / PTM_RATIO));
	screenEdgeBody->CreateFixture(&boxShapeDef);

	//right
	groundBox.SetTwoSided(b2Vec2(blit3D->screenWidth / PTM_RATIO,
		0), b2Vec2(blit3D->screenWidth / PTM_RATIO, blit3D->screenHeight / PTM_RATIO));
	screenEdgeBody->CreateFixture(&boxShapeDef);

	//______MAKE A BALL_______________________
	BallEntity *ball = MakeBall(ballSprite);

	//move the ball to the center bottomof the window
	b2Vec2 pos(blit3D->screenWidth / 2, 35);
	pos = Pixels2Physics(pos);//convert coordinates from pixels to physics world
	ball->body->SetTransform(pos, 0.f);

	//add the ball to our list of (ball) entities
	ballEntityList.push_back(ball);

	std::string LoadedMap = "";

	
	int i = rand() % 3;

	switch(i)
	{
	case 1:
		LoadedMap = "Media//beach.txt";
		break;
	case 2:
		LoadedMap = "Media//man.txt";
		break;

	case 3:
		LoadedMap = "Media//junk.txt";
		break;
	}

	LoadMap(LoadedMap);
	// Create contact listener and use it to collect info about collisions
	contactListener = new MyContactListener();
	world->SetContactListener(contactListener);

	//paddle
	cursorX = blit3D->screenWidth / 2;
	paddleEntity = MakePaddle(blit3D->screenWidth / 2, 30, paddleSprite);
	entityList.push_back(paddleEntity);
}

void DeInit(void)
{
	//delete all the entities
	for (auto &e : entityList) delete e;
	entityList.clear();

	for (auto &b : ballEntityList) delete b;
	ballEntityList.clear();

	for (auto& p : particleList) delete p;
	particleList.clear();

	//delete the contact listener
	delete contactListener;

	//Free all physics game data we allocated
	delete world;

	//any sprites/fonts still allocated are freed automatically by the Blit3D object when we destroy it
}

void Update(double seconds)
{
	camera->Update((float)seconds);
	//stop it from lagging hard if more than a small amount of time has passed
	if (seconds > 1.0 / 30) elapsedTime += 1.f / 30;
	else elapsedTime += (float)seconds;

	//move paddle to where the cursor is
	b2Vec2 paddlePos;
	paddlePos.y = 30;
	paddlePos.x = cursorX;
	paddlePos = Pixels2Physics(paddlePos);
	paddleEntity->body->SetTransform(paddlePos, 0);

	//if ball is attached to paddle, move ball to where paddle is
	if (attachedBall)
	{
		assert(ballEntityList.size() == 1); //make sure there is exactly one ball
		b2Vec2 ballPos = paddleEntity->body->GetPosition();
		ballPos.y = 64 / PTM_RATIO;
		ballEntityList[0]->body->SetTransform(ballPos, 0);
	}

	//don't apply physics unless at least a timestep worth of time has passed
	while (elapsedTime >= timeStep)
	{

		//update the particle list and remove dead particles
		for (int i = particleList.size() - 1; i >= 0; --i)
		{
			if (particleList[i]->Update(timeStep))
			{
				//time to die!
				delete particleList[i];
				particleList.erase(particleList.begin() + i);
			}
		}

		powerCooldown -= timeStep;
		if (powerCooldown < 0)
			powerCooldown = 0;


		//update the physics world
		world->Step(timeStep, velocityIterations, positionIterations);

		// Clear applied body forces. 
		world->ClearForces();

		elapsedTime -= timeStep;

		std::vector<Entity *> deadEntityList; //dead entities
											  //loop over contacts

		int numBalls = ballEntityList.size();

		for (int pos = 0; pos < contactListener->contacts.size(); ++pos)
		{
			MyContact contact = contactListener->contacts[pos];

			//fetch the entities from the body userdata
			Entity *A = (Entity *)contact.fixtureA->GetBody()->GetUserData().pointer;
			Entity *B = (Entity *)contact.fixtureB->GetBody()->GetUserData().pointer;

			if (A != NULL && B != NULL) //if there is an entity for these objects...
			{


				if (A->typeID == ENTITYBALL)
				{
					//swap A and B
					Entity *C = A;
					A = B;
					B = C;
				}

				if (B->typeID == ENTITYBALL && A->typeID == ENTITYBRICK)
				{
					BrickEntity *be = (BrickEntity *)A;
					camera->Shake(10);

					//spawn particles
								for (int particleCount = 0; particleCount < 10; ++particleCount)
			{

				Particle *p = new Particle();
				p->coords = Physics2Pixels(A->body->GetPosition());
				p->angle = rand() % 360;
				p->direction = deg2vec(rand() % 360);
				p->rotationSpeed = (float)(rand() % 1000) / 100 - 5;
				p->startingSpeed = rand() % 200;
				p->targetSpeed = rand() % 200;
				p->totalTimeToLive = 0.3f;

				p->startingScaleX = (float)(rand() % 100) / 200 + 0.1;
				p->startingScaleY = (float)(rand() % 100) / 200 + 0.1;
				p->targetScaleX = (float)(rand() % 100) / 1000 + 0.05;
				p->targetScaleY = (float)(rand() % 100) / 1000 + 0.05;

				p->spriteList.push_back(debrisList[(rand() % 3)]);
				particleList.push_back(p);
			}

					if (be->HandleCollision())
					{

						//we need to remove this brick from the world, 
						//but can't do that until all collisions have been processed
						deadEntityList.push_back(be);
					}
				}

				if (B->typeID == ENTITYBALL && A->typeID == ENTITYPADDLE)
				{
					PaddleEntity *pe = (PaddleEntity *)A;
					//bend the ball's flight
					pe->HandleCollision(B->body);
				}

				if (B->typeID == ENTITYBALL && A->typeID == ENTITYGROUND)
				{
					if (numBalls > 1)
					{
						//remove this ball, but we have others
						deadEntityList.push_back(B);
						numBalls--;
					}
					else
					{
						//lose a life
						lives--;
						camera->Shake(40);
						attachedBall = true; //attach the ball for launching
						ballEntityList[0]->body->SetLinearVelocity(b2Vec2(0.f, 0.f));
						if (lives < 1)
						{
							//gameover
							gameState = START;
						}
					}
				}
			}//end of checking if they are both NULL userdata
		}//end of collison handling

		 //clean up dead entities
		for (auto &e : deadEntityList)
		{
			//remove body from the physics world and free the body data
			world->DestroyBody(e->body);
			//remove the entity from the appropriate entityList
			if (e->typeID == ENTITYBALL)
			{
				for (int i = 0; i < ballEntityList.size(); ++i)
				{
					if (e == ballEntityList[i])
					{
						delete ballEntityList[i];
						ballEntityList.erase(ballEntityList.begin() + i);
						break;
					}
				}
			}
			else
			{
				for (int i = 0; i < entityList.size(); ++i)
				{
					if (e == entityList[i])
					{
						delete entityList[i];
						entityList.erase(entityList.begin() + i);
						break;
					}
				}
			}
		}

		deadEntityList.clear();
	}

}

void Draw(void)
{
	
	glClearColor(0.8f, 0.6f, 0.7f, 0.0f);	//clear colour: r,g,b,a 	
											// wipe the drawing surface clear
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	camera->Draw();

	switch (gameState)
	{
	case START:
		logo->Blit(blit3D->screenWidth / 2, blit3D->screenHeight / 2);
		break;

	case PLAYING:
		//loop over all entities and draw them
		for (auto &e : entityList) e->Draw();
		for (auto &b : ballEntityList) b->Draw();
		for (auto& p : particleList) p->Draw();

		std::string lifeString = "Lives: " + std::to_string(lives);
		std::string ScoreString = "Score: " + std::to_string(score);
		debugFont->BlitText(20, 30, lifeString);
		debugFont->BlitText(20, 60, ScoreString);
		break;
	}
}

//the key codes/actions/mods for DoInput are from GLFW: check its documentation for their values
void DoInput(int key, int scancode, int action, int mods)
{
	//two powerups, manually activated. multiball and speedup

	if (key == GLFW_KEY_1) //powerup 1
	{
		if(powerCooldown == 0)
		{
		}
		//spawn 5 balls at loc of ball
	}

	if (key == GLFW_KEY_2)
	{
		if (powerCooldown == 0)
		{

		}

	}

	if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		blit3D->Quit(); //start the shutdown sequence
}

void DoCursor(double x, double y)
{
	//scale display size
	cursorX = static_cast<float>(x) * (blit3D->screenWidth / blit3D->trueScreenWidth);
}

void DoMouseButton(int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		switch (gameState)
		{
		case START:
			gameState = PLAYING;
			attachedBall = true;
			lives = 3;
			break;

		case PLAYING:
			if (attachedBall)
			{
				camera->Shake(20);
				attachedBall = false;
				//launch the ball

				//safety check
				assert(ballEntityList.size() == 1); //make sure there is exactly one ball	

				Entity *b = ballEntityList[0];
				//kick the ball in a random direction upwards			
				b2Vec2 dir = deg2vec(90 + plusMinus5Degrees(rng)); //between 85-95 degrees
														//make the ball move
				dir *= 256.f; //scale up the velocity
				b->body->SetLinearVelocity(dir); //apply velocity to kinematic object				
			}
			break;
		}
	}

}

void LoadMap(std::string fileName)
{
	//clear the current brickList
	for (auto B : entityList) delete B;
	entityList.clear();

	//open file
	std::ifstream myfile;
	myfile.open(fileName);

	if (myfile.is_open())
	{
		//read in # of bricks
		int brickNum = 0;
		myfile >> brickNum;

		//read in each brick
		for (; brickNum > 0; --brickNum)
		{
			//make a brick
			int t = 0;
			myfile >> t;
			BrickColour newCol = (BrickColour)t;
			int x = 0;
			int y = 0;
			myfile >> x;
			myfile >> y;

			BrickEntity* b = MakeBrick(newCol, x, y);
			entityList.push_back(b);
		}

		myfile.close();
	}
}


int main(int argc, char *argv[])
{
	//memory leak detection
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	blit3D = new Blit3D(Blit3DWindowModel::DECORATEDWINDOW, 1920, 1080);

	//set our callback funcs
	blit3D->SetInit(Init);
	blit3D->SetDeInit(DeInit);
	blit3D->SetUpdate(Update);
	blit3D->SetDraw(Draw);
	blit3D->SetDoInput(DoInput);
	blit3D->SetDoCursor(DoCursor);
	blit3D->SetDoMouseButton(DoMouseButton);

	//Run() blocks until the window is closed
	blit3D->Run(Blit3DThreadModel::SINGLETHREADED);
	if (blit3D) delete blit3D;
}