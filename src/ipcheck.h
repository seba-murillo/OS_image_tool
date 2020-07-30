/*
 * ipcheck.h
 *
 *  Created on: Mar 29, 2020
 *      Author: seba
 */

#ifndef IPCHECK_H_
#define IPCHECK_H_
#include <ctype.h>
#include <string.h>
#include <global.h>
#include <stdlib.h>

#define SIZE_IP 15 ///< maximum size of IP string (only IPv4 allowed)

// codigo robado (y adaptado) descaradamente de
// https://www.tutorialspoint.com/c-program-to-validate-an-ip-address

/// check if the given string contains a number
/// @param str string to check
/// @returns 1 if the string is a valid number, 0 if it is not
int validate_number(char* str){
	while(*str){
		if(!isdigit(*str)){ // if the character is not a number, return false
			return 0;
		}
		str++; //point to next character
	}
	return 1;
}

/// validates a string according to IPv4 format (XXX.XXX.XXX.XXX)
/// @param source string to validate
/// @returns 1 if the string is a valid IP, 0 if it is not
int validate_ip(char* source){ //check whether the IP is valid or not
	if(source == NULL) return FALSE;
	char IP[SIZE_IP];
	strncpy(IP, source, SIZE_IP); // copy IP, since if strtok fails if string is literal
	int num, dots = FALSE;
	char* ptr;
	ptr = strtok(IP, "."); // cut the string using dot delimiter
	if(ptr == NULL) return FALSE;
	while(ptr){
		if(!validate_number(ptr)) //check whether the sub string is holding only number or not
			return FALSE;
		num = (int) strtol(ptr, NULL, 10); // convert substring to number
		if(num >= 0 && num <= 255){
			ptr = strtok(NULL, "."); //cut the next part of the string
			if(ptr != NULL) dots++; //increase the dot count
		}else return FALSE;
	}
	if(dots != 3) // if the number of dots are not 3, return false
		return FALSE;
	return TRUE;
}
#endif /* IPCHECK_H_ */
