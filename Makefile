make: mmu.cpp
	g++ -std=c++0x mmu.cpp -o mmu
clean:
	rm -f mmu *~