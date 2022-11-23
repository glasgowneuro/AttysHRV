std::istream& operator >>(std::istream &inputStream, PPMObject &other)
{
    inputStream >> other.magicNum;
    inputStream >> other.width >> other.height >> other.maxColVal;
    inputStream.get(); // skip the trailing white space
    size_t size = other.width * other.height * 3;
    other.m_Ptr = new char[size];
    inputStream.read(other.m_Ptr, size);
    return inputStream;
}
