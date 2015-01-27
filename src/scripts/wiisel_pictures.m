RGB = imread('input_image.bmp', 'bmp');
fileID = fopen('output.txt','w');
for i = 1:30
    for j = 1:30
        fprintf(fileID,'writeLEDUncomp(%d, %d, %d, %d, %d);\n',i-1, j-1, RGB(i, j, 1), RGB(i, j, 2), RGB(i, j, 3));
    end
end
fclose(fileID);
