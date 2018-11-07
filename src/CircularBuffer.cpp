#include "circular_buffer.h"


CircularBuffer::CircularBuffer() {
	this.buffer = {0};
	this.i = 0;
	this.size = 0;
}

void CircularBuffer::write(T data) {
	this.buffer[this.i] = data;
	// This will likely be faster than the equivalent modulo statement below,
	// which uses division which is very slow on AVR.
	// this.i = (this.i + 1) % BUF_SIZE;
	if (++this.i >= this.capacity) this.i = 0;
}

void CircularBuffer::write(const T& data) {
	// TODO	The const ref argument type requires that the assignment operator
	//		for type T performs the necessary copy mechanics.
	this.buffer[this.i] = data;
	// This will likely be faster than the equivalent modulo statement below,
	// which uses division which is very slow on AVR.
	// this.i = (this.i + 1) % BUF_SIZE;
	if (++this.i >= this.capacity) this.i = 0;
}

void CircularBuffer::clear() {
	this.size = 0;
}

bool CircularBuffer::full() {
	return this.size >= BUF_SIZE;
}