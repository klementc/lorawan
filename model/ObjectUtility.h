#ifndef OBJECT_UTILITY_H
#define OBJECT_UTILITY_H

int OBJECT_SIZE_BYTES = 0; // size of the object to send in bytes
/**
 * ratio to use between the number of coded and uncoded fragments, used for the redundancy mechanism to permit loss
 * Described in https://lora-alliance.org/wp-content/uploads/2020/11/fragmented_data_block_transport_v1.0.0.pdf
 * Using results from Gallager: https://www.inference.org.uk/mackay/gallager/papers/ldpc.pdf
 * Principle is: An object of M fragments coded into N fragments can be recovered on average after the reception of M+2 coded fragments (so N-(M+2) fragments can be lost)
 * This means, a CR of 1/2 can allow almost half of the fragments to be lost during the multicast and still recovering the object in the end
 */
double CR = 1;

#endif