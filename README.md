# 2020-1-OSSP1-whalewhale-3
**공개sw프로젝트 1분반 고래고래조입니다.**\
팀원 : 김현도, 박희상, 이유경, 장석운

## 1. 프로젝트 명
openCV SURF를 이용한 명화 인식 프로그램

## 2. 프로젝트 개요
명화를 찍은 사진이나 이미지를 인식하여 명화의 정보를 출력하는 프로그램입니다. 

## 3. 프로젝트 목표

주변에 보이는 명화들을 이미지로 인식하여 그림의 작품명, 화가, 작품 설명 등 그림 정보 출력을 목표로 합니다.

 - 이미지 추출하기
     -> WEB GALLERY OF ART(wga.hu)에서 제공하는 48000여개의 이미지의 csv파일 데이터를 수집하여 그 안의 이미지 url을 이용한다.
     
 - OpenCV -> SURF 이용한 이미지 인식하기
     - SIFT(Scale Invariant Feature Transform) 원리
          - 이미지의 크기와 회전에 상관없이 불변하는 특징을 추출하는 알고리즘.
        명화의 사진이나 이미지를 가지고 명화를 찾아야하기 때문에 사진이 회전되어 있거나 크기 변화에도 서로 매칭될 수 있는 알고리즘을 사용한다.
         1) "Scale space" 만들기
         2) Difference of Gaussian (DoG) 연산
         3) keypoint들 찾기
         4) 나쁜 keypoint들 제거
         5) keypoint들에 방향 할당
         6) 최종 SIFT 특징 산출
         
         https://bskyvision.com/21 참조
         
         
## 4. 개발환경

Develop Tool : Visual Studio 2019(C++), OpenCV(ver.3.4.1ver)


## 5. 실행 방법

 1) opencv 설치
    - 3.4.1 버전으로 설치
         - 비주얼 스튜디오 2019의 64비트를 이용했는데 처음 install때 opencv_world부분에서 계속 오류(protoc 버전이 낮음으로 인해)가 나서 world기능을 빼고 다시 시도했지만 가장 중요한 모듈인 xfeatures2d부분에서 또 오류가 났다. 원인은 추가 종속성에서 xfeatures2dxxxd.lib를 넣어주지 않아서였다. xxx는 opencv의 버전으로 처음에 가장 최신 버젼인 4.3.0으로 설치했다가 오류의 원인을 찾기 위해 버젼을 3.4.1로 낮췄다.
         
  2) opencv_contrib를 다운
      - 3.4.1 버전으로 다운
     
  3) cmake 설치
     - SIFT를 지원하는 opencv 버전에 추가 기능(Xfeatures2d등)을 제공하는 opencv_contrib를 camke프로그램으로 추가한다.
     
  4) 설정하기 
      - https://webnautes.tistory.com/1329 영상을 참조하여 버전 설정만 3.4.1로 수정하여 설정한다.


## 6. 실행 화면




## 7. 사용 오픈소스

OpenCV : <https://github.com/opencv/opencv>

OpenCV SURF로 이미지 매칭 테스트 : <https://github.com/opencv/opencv_contrib/blob/master/modules/xfeatures2d/samples/surf_matcher.cpp>
                                <https://webnautes.tistory.com/1152>
