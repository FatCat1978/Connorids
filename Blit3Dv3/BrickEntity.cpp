#include "BrickEntity.h"
//#include "Camera.h"

//Decide whether to change colour or die off:
//return true if this pbject should be removed

extern int score;

//extern Camera2D* camera;

bool BrickEntity::HandleCollision()
{
	score++;
	//camera->Shake(20);
	bool retval = false;
	switch(colour)
	{
	case BrickColour::RED:
		colour = BrickColour::ORANGE;
		sprite = orangeBrickSprite;
		break;

	case BrickColour::ORANGE:
		colour = BrickColour::YELLOW;
		sprite = yellowBrickSprite;
		break;

	case BrickColour::YELLOW:
	default:
		retval = true;
		break;
	}

	return retval;
}

extern b2World *world;

BrickEntity * MakeBrick(BrickColour type, float xpos, float ypos)
{
	BrickEntity *brickEntity = new BrickEntity();
	brickEntity->colour = type;

	//set the sprite to draw with
	switch(type)
	{
	case BrickColour::RED:
		brickEntity->sprite = redBrickSprite;
		break;

	case BrickColour::ORANGE:
		brickEntity->sprite = orangeBrickSprite;
		break;

	case BrickColour::YELLOW:
		brickEntity->sprite = yellowBrickSprite;
		break;
	}

	//make the physics body
	b2BodyDef brickBodyDef;

	//set the position of the center of the body, 
	//converting from pxel coords to physics measurements
	brickBodyDef.position.Set(xpos / PTM_RATIO, ypos / PTM_RATIO);
	brickBodyDef.type = b2_kinematicBody; //make it a kinematic body i.e. one moved by us

	//make the userdata point back to this entity
	brickBodyDef.userData.pointer = reinterpret_cast<uintptr_t>(brickEntity);

	brickEntity->body = world->CreateBody(&brickBodyDef); //create the body and add it to the world

	// Define a box shape for our dynamic body.
	b2PolygonShape boxShape;
	//SetAsBox() takes as arguments the half-width and half-height of the box
	boxShape.SetAsBox(64.0f / (2.f*PTM_RATIO), 32.0f / (2.f*PTM_RATIO));

	b2FixtureDef brickFixtureDef;
	brickFixtureDef.shape = &boxShape;
	brickFixtureDef.density = 1.0f; //won't matter, as we made this kinematic
	brickFixtureDef.restitution = 0;
	brickFixtureDef.friction = 0.1f;

	brickEntity->body->CreateFixture(&brickFixtureDef);

	return brickEntity;
}