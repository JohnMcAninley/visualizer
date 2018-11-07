/*
*	CircularBuffer is an implementation of a circular buffer designed
*	specifically for embedded systems. For this reason, its design,
*	implementation, and set of features differences from a standard 
*	implementation of a circular buffer.
*/
template <typename T>
class CircularBuffer {
	
	public:
		CircularBuffer(size_t capacity);
		~CircularBuffer(void);
		void write(T);
		T read();
		bool full();
		void clear();

		//T operator[](size_t index);
		T& operator[](size_t index);
		const T& operator[](size_t index);



	private:
		unsigned char buffer[BUF_SIZE]; // TODO
		unsigned char i;
		unsigned char capacity;
}