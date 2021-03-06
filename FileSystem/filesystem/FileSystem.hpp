#pragma once
#include "iosystem\IOSystem.hpp"
#include "Metadata.hpp"
#include "OFT.hpp"

/*information
  1)only the 3 first letters of file name are saved. Files with shorter filenames now can crash the app.
  

 */
 /*bugs to solve
 1)after stress testing by adding a lot of files (much more than 10) throws vector exception. May be connected with int/char conversations.


 */
 /*done
	simple console visualization   // metadataToPrettyString();
	file creation  
	ability to change/destroy_last/push_back blocks of a particular file.
 */
/*todo
	-destroying files;
	-working with files (open, close, etc...);
	-handle or get rid of exceptions (from Metadata class)

*/



template <int k, int descriptorLength>
class FileSystem {
//data
	 IOSystem &io;
	 Metadata<k, descriptorLength> meta;
	std::vector<char> buff;

public:
	std::string metadataToPrettyString();
	FileSystem(IOSystem& io);
	void clear();
	//rewrites only already allocated blocks. blockNumber is the number of block in a file.
	bool rewriteBlock(int fileDescrNum, int blockNumber, std::vector<char>& data);
	//reads only already allocated blocks. blockNumber is the number of block in a file.
	bool readBlock(int fileDescrNum, int blockNumber, std::vector<char>& data);
	//adds block to the end of file. blockNumber is the number of block in a file.
	bool addBlock(int fileDescrNum, std::vector<char>& data);
	//removes last block from the end of the file. todo.
	bool removeLastBlock(int fileDescrNum);
	//creates new file by allocating file descriptor and registering in the directory;
	bool createFile(std::string name);

	// returns the number of the descriptor if found, if not - returns 0;
	int getDescriptorByFileName(std::string name);
private:
	
	
};


int getInt(int pos, std::vector<char>& buff) {
	return ((buff[pos * 4] << 24) | (buff[pos * 4 + 1] << 16) | (buff[pos * 4 + 2] << 8) | buff[pos * 4 + 3]);
}

template<int k, int descriptorLength>
std::string FileSystem<k, descriptorLength>::metadataToPrettyString()
{	
	std::string s = "\n";
	s += "DIRECTORY\n";
	FileDescriptor<descriptorLength> d = meta.getDescriptor(0);
	int len = d.data[0];
	int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	//this block might not always work with different blockLength
	for (int i = 0; i < blockNum; i++){
		readBlock(0,i,buff);
		for (int j = 0; (j < io.getBlockLength() / 4) && (i*io.getBlockLength() + j*4 < len); j++) {
			if (j % 2) {
				s += std::to_string(getInt(j, buff)) + " |";
			} else {
				for (int t = 0; t < 4; t++) {
					s += buff[j * 4 + t];
				}
			}
			s += " ";
		}
		s += '\n';
		int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	}
	return meta.toPrettyString() + s;
}

template <int k, int descriptorLength>
FileSystem<k, descriptorLength>::FileSystem(IOSystem &io): io(io),meta(Metadata<k,descriptorLength>(io)){
		buff.resize(io.getBlockLength());
}
template<int k, int descriptorLength>
inline void FileSystem<k, descriptorLength>::clear(){
	meta.clear();
}

template<int k, int descriptorLength>
bool FileSystem<k, descriptorLength>::createFile(std::string name){
	//0) check the file name
	if (getDescriptorByFileName(name) != 0)return false;
	//1) find free descriptor
	int descriptorNum = meta.findFreeDescriptor();
	FileDescriptor<descriptorLength> dir = meta.getDescriptor(0);
	int len = dir.data[0];
	int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	readBlock(0, blockNum - 1, buff);
	len %= io.getBlockLength();
	int i = 0;
	while ((len%64!=0) && i<4) {
		buff[len] = name[i];
		len++;
		i++;
	}
	while ((len%64!=0) && i<8) {
		buff[len] = ((descriptorNum) >> (24 - 8 * (i - 4)) & 0xFF);
		len++;
		i++;
	}
	if(len>0)rewriteBlock(0, blockNum - 1, buff);
	//if(file is bigger now) make it bigger.
	if ((dir.data[0] + 8 +io.getBlockLength()-1) / io.getBlockLength() > (dir.data[0]+io.getBlockLength()-1) / io.getBlockLength()) {
		for (int j = 0; i < 8; i++, j++) {
			if (i < 4) {
				buff[j] = name[i];
			}
			else {
				buff[j] = (descriptorNum) >> ((24 - 8 * (i - 4)) & 0xFF);
			}
		}
		addBlock(0, buff);
	}
	meta.setDescriptorData(0, 0, dir.data[0] + 8);
	return true;
}

template<int k, int descriptorLength>
inline int FileSystem<k, descriptorLength>::getDescriptorByFileName(std::string name){
	FileDescriptor<descriptorLength> d = meta.getDescriptor(0);
	int len = d.data[0];
	int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	for (int i = 0; i < blockNum; i++) {
		readBlock(0, i, buff);
		for (int j = 0; (j < io.getBlockLength() / 8) && (i*io.getBlockLength() + j * 8 < len); j++) {
			bool compareVal = true;
			for (int t = 0; (t < 4) && compareVal; t++)
				if (buff[j * 8 + t] != name[t]) compareVal = false;
			if (compareVal) {
				return getInt(j * 2 + 1, buff);
			}
		}
		int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	}
	return 0;
}

template<int k, int descriptorLength>
bool FileSystem<k, descriptorLength>::rewriteBlock(int fileDescrNum, int blockNumber, std::vector<char>& data) {
	FileDescriptor<descriptorLength> d = meta.getDescriptor(fileDescrNum);
	int len = d.data[0];
	int blockNum = (len + (io.getBlockLength()-1)) / io.getBlockLength();
	if (blockNum == 0 || blockNum <=blockNumber) return false;
	if (blockNumber < (descriptorLength - 1) || ((blockNumber == descriptorLength - 1) && (blockNum == descriptorLength-1))) {
		io.writeBlock(d.data[blockNumber + 1], data);
	}
	else {
		while ((blockNumber > (descriptorLength - 1))|| (blockNumber ==(descriptorLength-1))&& (blockNum> (descriptorLength - 1))) {
			d = meta.getDescriptor(d.data[descriptorLength - 1]);
			blockNumber -= (descriptorLength - 1);
			blockNum -= (descriptorLength - 1);
		}
		io.writeBlock(d.data[blockNumber], data);
	}
	return true;
}

template<int k, int descriptorLength>
bool FileSystem<k, descriptorLength>::readBlock(int fileDescrNum, int blockNumber, std::vector<char>& data) {
	FileDescriptor<descriptorLength> d = meta.getDescriptor(fileDescrNum);
	int len = d.data[0];
	int blockNum = (len + (io.getBlockLength() - 1)) / io.getBlockLength();
	if (blockNum == 0 || blockNum <= blockNumber) return false;
	if (blockNumber <= (descriptorLength - 1)) {
		io.readBlock(d.data[blockNumber + 1], data);
	}
	else {
		while ((blockNumber > (descriptorLength - 1)) || (blockNumber == (descriptorLength - 1)) && (blockNum> (descriptorLength - 1))) {
			d = meta.getDescriptor(d.data[descriptorLength - 1]);
			blockNumber -= (descriptorLength - 1);
			blockNum -= (descriptorLength - 1);
		}
		io.readBlock(d.data[blockNumber], data);
	}
	return true;
}

template<int k, int descriptorLength>
bool FileSystem<k, descriptorLength>::addBlock(int fileDescrNum, std::vector<char>&data) {
	int dataBlock = meta.findFreeBlock();
	io.writeBlock(dataBlock, data);
	FileDescriptor<descriptorLength> d = meta.getDescriptor(fileDescrNum);
	int len = d.data[0];
	int blockNum = (len + (io.getBlockLength()-1)) / io.getBlockLength();

	int currDescrNum = fileDescrNum;
	if (blockNum < (descriptorLength - 1)) {
		d.data[blockNum + 1] = dataBlock;
		meta.setDescriptor(currDescrNum, d);
	}
	else if (blockNum % (descriptorLength - 1) == 0) {
		while (blockNum != (descriptorLength - 1)) {
			currDescrNum = d.data[descriptorLength - 1];
			d = meta.getDescriptor(currDescrNum);
			blockNum -= (descriptorLength - 1);
		}
		int tData = d.data[descriptorLength - 1];
		d.data[descriptorLength - 1] = meta.findFreeDescriptor();
		meta.setDescriptor(currDescrNum, d);
		currDescrNum = d.data[descriptorLength - 1];
		d = meta.getDescriptor(currDescrNum);
		d.data[0] = tData;
		d.data[1] = dataBlock;
		meta.setDescriptor(currDescrNum, d);
	}
	else {
		while (blockNum  >(descriptorLength - 2)) {
			currDescrNum = d.data[descriptorLength - 1];
			d = meta.getDescriptor(currDescrNum);
			blockNum -= (descriptorLength - 1);
		}
		d.data[blockNum + 1] = dataBlock;
		meta.setDescriptor(currDescrNum, d);
	}
	return true;
}

template<int k, int descriptorLength>
bool FileSystem<k, descriptorLength>::removeLastBlock(int fileDescrNum) {
 	FileDescriptor<descriptorLngth> d = meta.getDescriptor(fileDescrNum);
	int len = d.data[0];
	if (len == 0) return false;
	int blockNum = (len + (io.getBlockLength()-1)) / io.getBlockLength();
	int currDescrNum = fileDescrNum;
	if (blockNum < descriptorLength) {
		meta.freeBlock(d.data[blockNum - 1]);
	}
	else if (blockNum % (descriptorLength - 1) == 1) {
		while (blockNum != (descriptorLength - 1)) {
			currDescrNum = d.data[descriptorLength - 1];
			d = meta.getDescriptor(currDescrNum);
			blockNum -= (descriptorLength - 1);
		}
		FileDescriptor<descriptorLngth> d1 = meta.getDescriptor(d.data[(descriptorLength - 1)]);
		int dataToTransfer = d1.data[0];
		meta.freeBlock(d1.data[1]);
		meta.freeDescriptor(d.data[descriptorLength - 1]);
		d.data[descriptorLength - 1] = dataToTransfer;
		meta.setDescriptor(currDescrNum, d);
	} else {
		while (blockNum  >(descriptorLength - 1)) {
			currDescrNum = d.data[descriptorLength - 1];
			d = meta.getDescriptor(d.data[(descriptorLength - 1)]);
		    blockNum -= (descriptorLength - 1);
		}
		meta.freeBlock(d.data[blockNum]);
	}
	return true; 
}


