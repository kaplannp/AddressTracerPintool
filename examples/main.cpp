#include <iostream>

int sum(int* arr, int n){
  int sum = 0;
  for(int i = 0; i < n; i++){
    sum += arr[i];
  }
  return sum;
}

int main(){
  int n = 100;
  int* arr = new int[n];
  for (int i=0; i<n; i++){
    arr[i] = i;
  }
  int total = sum(arr, n);
  return 0;
}

