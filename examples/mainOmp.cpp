#include <iostream>
#include <omp.h>

int sum(int* arr, int n, int m){
  int sum = 0;
  #pragma omp parallel
  {
    printf("Thread %d launched\n", omp_get_thread_num());
    //This loop should cause a lot of reads/writes to the same address from diff
    //threads
    #pragma omp for
    for (int j = 0; j < m; j++){
      for(int i = 0; i < n; i++){
        sum += arr[i];
      }
    }
  }
  return sum;
}

int main(){
  int n = 100;
  int* arr = new int[n];
  for (int i=0; i<n; i++){
    
    arr[i] = i;
  }
  int total = sum(arr, n, 64);
  return 0;
}

