#include <iostream>
#include <fstream>
#include <mpi.h>
#include <chrono>
#include <cmath>

using namespace std;

#define MCW MPI_COMM_WORLD

struct Complex {
    double r;
    double i;
};

Complex operator + (Complex s, Complex t){
    Complex v;
    v.r = s.r + t.r;
    v.i = s.i + t.i;
    return v;
}

Complex operator * (Complex s, Complex t){
    Complex v;
    v.r = s.r*t.r - s.i*t.i;
    v.i = s.r*t.i + s.i*t.r;
    return v;
}

int rcolor(int iters, int maxIters){
    if(iters == maxIters) return 0;
    return 32*(iters%8);
}

int gcolor(int iters, int maxIters){
    if(iters == maxIters) return 0;
    return 32*(iters%8);
}

int bcolor(int iters, int maxIters){
    if(iters == maxIters) return 255;
    return 255 - (32*(iters%8));
}

int mbrot(Complex c, int maxIters){
    int i=0;
    Complex z;
    z=c;
    while(i<maxIters && z.r*z.r + z.i*z.i < 4){
        z = z*z + c;
        i++;
    }
    return i;
}

void byLine(int DIM, int line, Complex c1, Complex c2, ofstream& fout, int maxIters){
    int iterations[DIM+1];
    iterations[0] = line;
    Complex c;
    for(int i = DIM; i > 0; i--){
        // calculate one pixel of the DIM x DIM image
        c.r = (i*(c1.r-c2.r)/DIM)+c2.r;
        c.i = (line*(c1.i-c2.i)/DIM)+c2.i;
        int iters = mbrot(c,maxIters);
        iterations[i] = iters;
    }
    MPI_Send(iterations, DIM+1, MPI_INT, 0, 0, MCW);
}

int main(int argc, char **argv){
    int size, rank, length, data;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MCW, &rank);
    MPI_Comm_size(MCW, &size);
    MPI_Status status;
    ofstream fout;

    //below are the variables that can be changed to porduce a different image
    int maxIters = 255;
    Complex c1,c2;
    c1.r = -1.5;
    c1.i = -1;
    c2.r = .5;
    c2.i = 1;
    int DIM = 512;
    
    if(rank == 0){
        time_t start = clock(); 
        cout << "Starting Timer" << endl;

        int storage[DIM][DIM];
        int completed[DIM];
        fout.open("result.ppm");

        fout << "P3"<< endl;
        fout << DIM << " " << DIM << endl;
        fout << "255" << endl;

        //send initial lines
        for(int i = 1; i < size; i++){
            data = DIM-i+1;
            MPI_Send(&data, 1, MPI_INT, i, 0, MCW);
        }

        int iterations[DIM+1];
        for(int i = DIM-size+1; i > 0; i--){
            //recieve lines from processes
            MPI_Recv(iterations, DIM+1, MPI_INT, MPI_ANY_SOURCE, 0, MCW, &status);

            //send new work back until there are no lines left
            data = i;
            MPI_Send(&data, 1, MPI_INT, status.MPI_SOURCE, 0, MCW);

            //get the pixel values for each iteration
            for(int k = DIM-1; k >= 0; k--){
                storage[iterations[0]-1][k] = iterations[k];
            }
            fout << endl;
        }

        //recieve the left overs
        for(int i = 0; i < size-1; i++){
            MPI_Recv(iterations, DIM+1, MPI_INT, MPI_ANY_SOURCE, 0, MCW, MPI_STATUS_IGNORE);
            for(int k = DIM-1; k >= 0; k--){
                storage[iterations[0]-1][k] = iterations[k];
            }
        }

        for(int o = 0; o < DIM; o++){
            for(int k = DIM; k > 0; k--){
                int iters = storage[o][k];
                fout << rcolor(iters, maxIters)<<" ";
                fout << gcolor(iters, maxIters)<<" ";
                fout << bcolor(iters, maxIters)<<" ";
            }
        }

        //kill all processes
        data = -1;
        for(int i = 1; i < size; i++){
            MPI_Send(&data, 1, MPI_INT, i, 0, MCW);
        }
        fout.close();

        //stop timer and print time (timing code was taken from here: https://stackoverflow.com/questions/12231166/timing-algorithm-clock-vs-time-in-c)
        time_t stop = clock(); 
        double time = difftime(stop, start) / 1000000.0;
        time = ceil(time * 100.0) / 100.0;
        cout << "Execution took " << time << " seconds" << endl;
    
    //if not process 0
    } else {
        while(data != -1){
            //recieve line to work on
            MPI_Recv(&data, 1, MPI_INT, 0, 0, MCW, MPI_STATUS_IGNORE);

            if(data != -1){
                //do the work on the line
                byLine(DIM, data, c1, c2, fout, maxIters);
            }
        }
    }    

	MPI_Finalize();

    return 0;
}